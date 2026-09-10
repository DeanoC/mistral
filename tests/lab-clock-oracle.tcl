# Run in separate empty build directories: quartus_sh -t /path/lab-clock-oracle.tcl pos|neg LAB|MLAB
package require ::quartus::project
set fixture [file dirname [file normalize [info script]]]
set edge [lindex $argv 0]
if {$edge ni {pos neg}} { error "expected pos or neg" }
set block [lindex $argv 1]
if {$block ni {LAB MLAB}} { error "expected LAB or MLAB" }
set x [expr {$block eq "LAB" ? 7 : 8}]
project_new top -overwrite
set_global_assignment -name FAMILY "Cyclone V"
set_global_assignment -name DEVICE 5CSEBA6U23I7
set_global_assignment -name TOP_LEVEL_ENTITY top
set_global_assignment -name VERILOG_FILE [file join $fixture lab-clock-$edge.v]
set_global_assignment -name SDC_FILE clocks.sdc
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
set_global_assignment -name GENERATE_RBF_FILE ON
set_global_assignment -name STRATIXV_CONFIGURATION_SCHEME "PASSIVE SERIAL"
set_global_assignment -name ENABLE_CONFIGURATION_PINS OFF
set_global_assignment -name NUM_PARALLEL_PROCESSORS 4
set_location_assignment PIN_V11 -to FPGA_CLK1_50
set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to FPGA_CLK1_50
set_instance_assignment -name PLL_COMPENSATION_MODE DIRECT -to "*pll*|*"
set_location_assignment FF_X${x}_Y32_N4 -to ffa
set_location_assignment FF_X${x}_Y32_N10 -to ffb
set_location_assignment FF_X${x}_Y32_N16 -to ffc
set sdc [open clocks.sdc w]
puts $sdc {create_clock -name FPGA_CLK1_50 -period 20.000 [get_ports {FPGA_CLK1_50}]}
puts $sdc {derive_pll_clocks}
puts $sdc {derive_clock_uncertainty}
close $sdc
export_assignments
project_close
