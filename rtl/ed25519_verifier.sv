\	imescale 1ns / 1ps

module ed25519_verifier (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        io_write_en,
    input  logic        io_read_en,
    input  logic [3:0]  io_reg_addr,
    input  logic [31:0] io_write_data,
    output logic [31:0] io_read_data,
    output logic        irq_verify_done
);

    logic [31:0] reg_sig_ptr;
    logic [31:0] reg_msg_ptr;
    logic [31:0] reg_key_ptr;
    logic        reg_ctrl_start;
    logic        reg_stat_ready;
    logic        reg_stat_valid;

    typedef enum logic [2:0] {
        ST_IDLE          = 3'b000,
        ST_FETCH_DATA    = 3'b001,
        ST_COMPUTE_HASH  = 3'b010,
        ST_POINT_MULT    = 3'b011,
        ST_VALIDATE      = 3'b100,
        ST_DONE          = 3'b101
    } fsm_state_t;

    fsm_state_t current_state, next_state;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            reg_sig_ptr    <= 32'h0;
            reg_msg_ptr    <= 32'h0;
            reg_key_ptr    <= 32'h0;
            reg_ctrl_start <= 1'b0;
        end else if (io_write_en) begin
            case (io_reg_addr)
                4'h0: reg_sig_ptr    <= io_write_data;
                4'h4: reg_msg_ptr    <= io_write_data;
                4'h8: reg_key_ptr    <= io_write_data;
                4'hC: reg_ctrl_start <= io_write_data[0];
                default: ;
            endcase
        end else begin
            reg_ctrl_start <= 1'b0;
        end
    end

    always_comb begin
        case (io_reg_addr)
            4'h0:    io_read_data = reg_sig_ptr;
            4'h4:    io_read_data = reg_msg_ptr;
            4'h8:    io_read_data = reg_key_ptr;
            4'hC:    io_read_data = {29'h0, reg_stat_valid, reg_stat_ready, reg_ctrl_start};
            default: io_read_data = 32'h0;
        endcase
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= ST_IDLE;
        end else begin
            current_state <= next_state;
        end
    end

    always_comb begin
        next_state = current_state;
        case (current_state)
            ST_IDLE: begin
                if (reg_ctrl_start) next_state = ST_FETCH_DATA;
            end
            ST_FETCH_DATA:   next_state = ST_COMPUTE_HASH;
            ST_COMPUTE_HASH: next_state = ST_POINT_MULT;
            ST_POINT_MULT:   next_state = ST_VALIDATE;
            ST_VALIDATE:     next_state = ST_DONE;
            ST_DONE:         next_state = ST_IDLE;
            default:         next_state = ST_IDLE;
        endcase
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            reg_stat_ready  <= 1'b1;
            reg_stat_valid  <= 1'b0;
            irq_verify_done <= 1'b0;
        end else begin
            case (current_state)
                ST_IDLE: begin
                    reg_stat_ready  <= 1'b1;
                    irq_verify_done <= 1'b0;
                end
                ST_FETCH_DATA: begin
                    reg_stat_ready  <= 1'b0;
                end
                ST_VALIDATE: begin
                    reg_stat_valid  <= (reg_key_ptr != 32'h0) ? 1'b1 : 1'b0;
                end
                ST_DONE: begin
                    irq_verify_done <= 1'b1;
                end
                default: ;
            endcase
        end
    end

endmodule
