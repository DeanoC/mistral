module top(input wire FPGA_CLK1_50);
    wire duty_clock, locked;
    wire [31:0] gpo;
    wire qa, qb, qc;
    altera_pll #(
        .reference_clock_frequency("50.0 MHz"), .number_of_clocks(1),
        .output_clock_frequency0("25.0 MHz"), .phase_shift0("0 ps"),
        .duty_cycle0(75), .operation_mode("direct"),
        .fractional_vco_multiplier("false")
    ) pll (.refclk(FPGA_CLK1_50), .rst(1'b0), .outclk(duty_clock), .locked(locked));
    cyclonev_ff ffa (.d(gpo[0]), .clk(duty_clock), .ena(gpo[1]), .clrn(1'b1), .aload(1'b0), .sclr(1'b0), .sload(1'b0), .asdata(1'b0), .q(qa));
    cyclonev_ff ffb (.d(gpo[0]), .clk(duty_clock), .ena(gpo[2]), .clrn(1'b1), .aload(1'b0), .sclr(1'b0), .sload(1'b0), .asdata(1'b0), .q(qb));
    cyclonev_ff ffc (.d(gpo[0]), .clk(duty_clock), .ena(gpo[3]), .clrn(1'b1), .aload(1'b0), .sclr(1'b0), .sload(1'b0), .asdata(1'b0), .q(qc));
    cyclonev_hps_interface_mpu_general_purpose hps_gp (
        .gp_in({16'hD718, 12'b0, locked, qc, qb, qa}), .gp_out(gpo));
endmodule
