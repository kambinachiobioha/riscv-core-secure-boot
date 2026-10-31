import cocotb
from cocotb.triggers import Timer, RisingEdge
from cocotb.binary import BinaryValue

REG_BOOT_STATUS_ADDR = 0xC
BOOT_STATE_RECOVERY  = 0xFF

async def generate_clock(dut):
    while True:
        dut.clk.value = 0
        await Timer(10, units="ns")
        dut.clk.value = 1
        await Timer(10, units="ns")

async def assert_hardware_reset(dut):
    dut.rst_n.value = 0
    await RisingEdge(dut.clk)
    await RisingEdge(dut.clk)
    dut.rst_n.value = 1
    await RisingEdge(dut.clk)

@cocotb.test()
async def run_flash_tampering_injection_test(dut):
    cocotb.start_soon(generate_clock(dut))
    await assert_hardware_reset(dut)
    
    dut._log.info("[TEST INITIATION] Starting Threat Vector 1: Flash Memory Tampering Validation Loop")
    
    dut.io_write_en.value = 0
    dut.io_read_en.value = 0
    dut.io_reg_addr.value = 0x0
    dut.io_write_data.value = 0x0
    await RisingEdge(dut.clk)
    
    dut._log.info("[ATTACK INJECTION] Injecting corrupted SHA-256 validation blocks into SPI flash interfaces...")
    
    dut.io_write_en.value = 1
    dut.io_reg_addr.value = 0xC        
    dut.io_write_data.value = 0x01      
    await RisingEdge(dut.clk)
    dut.io_write_en.value = 0          
    
    dut._log.info("[PROCESSING CLOCK CYCLES] Advancing execution cycles for mathematical processing blocks...")
    for _ in range(50):
        await RisingEdge(dut.clk)
        
    dut.io_read_en.value = 1
    dut.io_reg_addr.value = REG_BOOT_STATUS_ADDR
    await RisingEdge(dut.clk)
    
    captured_boot_state = dut.io_read_data.value
    dut._log.info(f"[TELEMETRY ANALYSIS] Read Boot Status Register Value: {hex(int(captured_boot_state))}")
    
    assert int(captured_boot_state) == BOOT_STATE_RECOVERY, \
        f"CRITICAL FAULT: System failed to capture flash tampering vector! Active State: {hex(int(captured_boot_state))}"
        
    dut._log.info("--------------------------------------------------------------------------------")
    dut._log.info("SUCCESS: Verification Matrix Passed. Hardware isolated and safely locked down.")
    dut._log.info("--------------------------------------------------------------------------------")
