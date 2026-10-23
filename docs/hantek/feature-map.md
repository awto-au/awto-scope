# 2D82 firmware -- feature -> code map

Built by defining the firmware's debug strings in Ghidra and following xrefs to the
functions that reference them (`tmp/2d82/FeatureMap.java` -> `feature_map.txt`).
Partial (Ghidra resolved ~84/595 strings on the raw blob; a fuller pass with
manual data/xref analysis would complete it) -- but it already names the key handlers.

## Named functions (the important ones)

| Subsystem | Function | Evidence (debug strings it references) |
|---|---|---|
| **USB command parser** (the control protocol) | `FUN_080308de`, `FUN_08032ea0` | `USB_R_Buffer[2/3/5] = %d` |
| **Calibration engine** (V->units scaling -- the hub) | **`FUN_08031c40`** | `CaliParam.volt_cali_data[ch][range][pt]`, `bias_up/dn/zero`, `Serial_Number`, `dds_*_cali_data` |
| **ADC acquisition core** | `FUN_08018c18` | `chn_mode/ch/volt/adc_value` (up/down/zero) |
| **Voltage-scale / range** | `FUN_0802eba0` | `VoltScale[0/1] = %d` |
| **DC calibration mode** | `FUN_08028104` | "Calibration mode for DC A" |
| **Cal flags / persistence** | `FUN_08016c2c`, `FUN_08016bf8`, `FUN_0801615e` | `Htx_cali1_write_flag`, `OtherCaliFlag`, `volt_cali_data` writer |
| **A-channel cal factor** | `FUN_0802a3fc` | `A_Cal_Factor = %f` |

## What this unlocks
- **Control protocol** -> reverse `FUN_080308de`/`FUN_08032ea0` (the `USB_R_Buffer`
  command dispatch) to recover the command set -> write a host driver.
- **Scaling/units** -> `FUN_08031c40` holds `volt_cali_data[ch][range][point]`: the
  exact ADC->volts calibration. This is the data that makes readings meaningful.
- **Acquisition** -> `FUN_08018c18` is the per-channel ADC read loop.

## To complete the map
Re-run with a deeper Ghidra pass: define ALL strings (force ASCII over the whole
`.rodata`), run Auto-Analysis "Create Address Tables" + "Decompiler Parameter ID",
then re-emit. That resolves the remaining ~510 strings (incl. the 102 test-preset
names -> their menu/render handlers, and `usb_cable_config`/`GLCD_ID` -> USB/LCD init).
