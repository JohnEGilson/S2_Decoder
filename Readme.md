<h1>DS Decoder</h1>
<p>DS Decoder is a free and open-source decoder (GPL-3.0) to convert raw Deep SOLO telemetry into human readable .json files</p>

<h2>Acknowledgements</h2>
<p>DS_Decoder code uses an existing codebase as a foundational starting point, modifying it to be used with a related Argo float model.</p> 
<p>[Original Project Name]: https://github.com/Greenwood1981/S2BGC_Decoder</p>

<h2>Requirements</h2>
<ul>
  <li>A Linux operating system is recommended</li>
  <li>Compile with GNU g++ v11.4 or newer</li>
  <li>c++ boost libraries</li>
  <li>The <b>DS_PATH</b> environmental variable must be defined prior to running the decoder.</li>
  <li>Modify config/default_meta.json to match your institution's information</li>
</ul>

<h2>Installation</h2>
<p>To install, clone this repo, and run make. g++ v11.4 or newer is recommended</p>

<h2>Operation</h2>
<p>DS_Decoder can be automated using a cronjob or run manually by a user from the command line.</p>

<h2>Cronjob Example</h2>
<pre>
DS_PATH="/path/DS_Decoder"
#  min hr dom mon dow command
  */15  *   *   *   * /path/DS_Decoder 2>&1
</pre>

<h2>Code summary</h2>

<h2>File Structure</h2>
<pre>
DS_Decoder/
├─ config/
│  ├─ config.json         (User configurable parameters - path definitions, etc..)
│  ├─ default_meta.json   (Default meta info assigned to a float)
│  ├─ mission.json        (DS Mission json schema)
│  ├─ Engineering_Data_3-7.json  (DS Engineering data versions)
│  ├─ diagnostic_3-7.json        (DS Engineering diagnostic dive data versions)
├─ data/
│  ├─ 6131/ (Following the example data provided)
│  │  ├─ 6131_meta.json   (float specific meta info added near the top of each .json file)
│  │  ├─ hex
│  │  │  ├─ 6131_000.hex  (hex files are placed in this subdirectory after being processed)
│  │  │  ├─ 6131_001.hex
│  │  ├─ json
│  │  │  ├─ L0
│  │  │  |    6131_L0_000.json (decoded .json file)
│  │  │  |    6131_L0_001.json (decoded .json file)
│  │  │  ├─ SBD
│  │  │  |    6131_SBD_000.json (decoded .json file created from SBD_DS.cpp)
│  │  │  |    6131_SBD_001.json (decoded .json file created from SBD_DS.cpp)
│  │  │  ├─ L1
│  │  │  |    6131_L1_000.json (decoded .json file created from L0toL1_DS.cpp)
│  │  │  |    6131_L1_001.json (decoded .json file created from L0toL1_DS.cpp)
│  ├─ 6xxx/
│  │  ├─ 6xxx_meta.json
│  │  ├─ hex
│  │  ├─ json
├─ incoming/
│  ├─ 6131_002.hex        (unprocessed hex files)
│  ├─ 6131_003.hex
├─ log/                   (daily log files are generated and placed in this subdirectory)
├─ src/                   (Decoder c++ source code)
</pre>

<h2>Hex files</h2>
<p>DS raw telemetry must first be converted into a .hex file before it is decoded. A single .hex file is created for each float cycle. The .hex file is capable of formatting data from SBD or RUDICS telemetry. All DS messages that belong to the same cycle are concatenated together and are ordered by SBD message ID.</p>
<p>Each message includes two header lines that include additional information about the SBD message or RUDICS session. Note the Iridium SBD doppler position in the header.</p>
<p>Following the header lines is the raw binary data, represented as two-digit zero padded hex values. Up to 32 bytes of data is included on each line</p>
<h3>Hex file example</h3>
<pre>
#          SN Cycle  Size IMEI   MOMSN MTMSN PID                  Date         Lat         Lon   Err
SBD,     6131,    0,  274,   0,     24,    0,  0, 2025,12,13, 22,24,21,  -43.00328,  111.99288,    2
58 01 0b 17 f3 00 00 00 14 20 67 00 00 80 00 00 69 00 00 01 44 92 4a 00 00 00 00 00 00 00 00 00
0b 00 fe ff 00 01 02 fe 03 04 03 00 ff 02 00 fe 2e f4 e0 f2 e1 01 ff 10 1e 2f 00 0f 11 ff 2f 0f
1f 20 ff 2f f2 e1 00 00 01 f0 f2 f0 f2 f1 fd 4f 1f 00 1f e2 1f 01 fe 21 f1 e0 02 1d 2d 3f 00 f2
fe 01 01 ff 00 00 ff 02 00 fe 01 00 01 f7 3b 24 20 9f 00 00 80 00 42 2c 00 00 01 4d 24 92 00 00
00 00 00 00 00 00 00 ff fe fd 02 03 00 01 01 fb f1 14 e7 eb 23 f9 0b ff ef fe 00 9f f7 fd 3f e5
00 4f d0 01 80 44 01 ef ee ff ff e6 ff 6f 79 5b f6 de 51 19 0f 0b 0a 00 00 03 fd 02 00 ff 01 00
fd fe ff ff 08 01 fe 01 00 fd f7 04 fe 06 03 f8 04 01 01 fc 04 02 00 fd 01 03 fb fe 02 ff fb 01
0a ff fc 03 02 fd fd 00 02 05 00 ff 01 ff ff fe fd 00 01 02 01 02 fe fd fc 09 00 ff fb f9 ff f9
0e 01 fd f5 05 0e 02 ff f5 fc 0e 00 ff 3b 24 3c 3d 3e
...
</pre>
<h2>JSON output</h2>
<p>The DS decoder is designed to output a single file for each float cycle. Any sections missing in the .json file should be interpreted as packets that have not yet been received. Sample .json files are provided.</a></p>

<h3>Float Specific metadata</h3>
<p>Each DS .json file begins with a collection of float specific metadata. This information is meant to be updated by the user and is placed in a file in a float's subdirectory <b>data/6xxx/6xxx_meta.json</b>. Default meta information is assigned each time a new float is processed. The default values may be updated in <b>config/default_meta.json</b>.</p>

```javascript
{
  "FILE_CREATION_DATE": "2025-05-19T12:20:01Z",
  "DECODER_VERSION": "1.00",
  "SCHEMA_VERSION": "0.1",
  "INTERNAL ID NUMBER": 4008,
  "DAC_ID NUMBER": 99999,
  "WMO_ID NUMBER": 9999999,
  "TRANSMISSION ID NUMBER": 9999,
  "INSTRUMENT TYPE": "SOLO_D",
  "PI": "",
  "OPERATING_INSTITUTION": "",
  "PROJECT_NAME": "",
  "PROGRAM_NAME": "",
  "FIRMWARE_VERSION": "",
  "PROFILE_NUMBER": 0
}
```

<h3>hex_summary</h3>
<p>A summary of raw telemetered data is included after the float metadata. Timestamps and telemetry statistics are summarized in a single line for each RUDICS file and SBD message that is received.</p>

```javascript
{
  "telemetry_summary": [
    { "PID":  0, "source": "SBD", "TIME": "2025-12-15T16:38:36Z", "momsn": 38, "size":  323, "sensor_ids": [240,1,32,144] },
    { "PID":  1, "source": "SBD", "TIME": "2025-12-15T16:38:46Z", "momsn": 39, "size":  335, "sensor_ids": [64] },
    { "PID":  2, "source": "SBD", "TIME": "2025-12-15T16:38:57Z", "momsn": 40, "size":  316, "sensor_ids": [16,186] },
    { "PID":  3, "source": "SBD", "TIME": "2025-12-15T16:39:04Z", "momsn": 41, "size":  325, "sensor_ids": [32] },
    { "PID":  4, "source": "SBD", "TIME": "2025-12-15T16:39:18Z", "momsn": 42, "size":  333, "sensor_ids": [48,218] },
    { "PID":  5, "source": "SBD", "TIME": "2025-12-15T16:39:37Z", "momsn": 43, "size":  333, "sensor_ids": [160,176,24,40,56,120,80,154,170] },
    { "PID":  6, "source": "SBD", "TIME": "2025-12-15T16:39:45Z", "momsn": 44, "size":  328, "sensor_ids": [20,36] },
    { "PID":  7, "source": "SBD", "TIME": "2025-12-15T16:40:10Z", "momsn": 45, "size":  283, "sensor_ids": [52,149,165] },
    { "PID":  8, "source": "SBD", "TIME": "2025-12-15T16:40:21Z", "momsn": 46, "size":  309, "sensor_ids": [181,2,64] },
    { "PID":  9, "source": "SBD", "TIME": "2025-12-15T16:40:34Z", "momsn": 47, "size":  269, "sensor_ids": [80,96,241] },
    { "PID": 10, "source": "SBD", "TIME": "2025-12-15T16:40:45Z", "momsn": 48, "size":  118, "sensor_ids": [226] },
    { "PID": 11, "source": "SBD", "TIME": "2025-12-15T16:41:11Z", "momsn": 49, "size":   58, "sensor_ids": [222] }
  ]
}
```

<h3>packet_info</h3>
<p>Each DS session may contain multiple packets of information. This section highlights each type of packets that have been received in a human readable format</p>

```javascript
{
 "packet_info": {
    "packet_count": 32,
    "packet_bytes": 3186,
    "packet_type": [
      { "id": "01", "bytes":   24, "packets":  1, "description": "GPS start-dive" },
      { "id": "02", "bytes":   24, "packets":  1, "description": "GPS end-dive" },
      { "id": "10", "bytes":  277, "packets":  1, "description": "CTD_Binned_Descend PRES" },
      { "id": "14", "bytes":  126, "packets":  1, "description": "CTD_Binned PRES" },
      { "id": "18", "bytes":   25, "packets":  1, "description": "CTD_Discrete_Descend PRES" },
      { "id": "20", "bytes":  505, "packets":  2, "description": "CTD_Binned_Descend TEMP" },
      { "id": "24", "bytes":  190, "packets":  1, "description": "CTD_Binned TEMP" },
      { "id": "28", "bytes":   25, "packets":  1, "description": "CTD_Discrete_Descend TEMP" },
      { "id": "30", "bytes":  293, "packets":  1, "description": "CTD_Binned_Descend PSAL" },
      { "id": "34", "bytes":  134, "packets":  1, "description": "CTD_Binned PSAL" },
      { "id": "38", "bytes":   25, "packets":  1, "description": "CTD_Discrete_Descend PSAL" },
      { "id": "40", "bytes":  513, "packets":  2, "description": "Engineering Fall" },
      { "id": "50", "bytes":  166, "packets":  2, "description": "Engineering Rise" },
      { "id": "60", "bytes":   94, "packets":  1, "description": "Engineering Pump" },
      { "id": "78", "bytes":   25, "packets":  1, "description": "CTD_Discrete_Descend elapsed_second" },
      { "id": "90", "bytes":   54, "packets":  1, "description": "CTD_Raw_Descend PRES" },
      { "id": "95", "bytes":   54, "packets":  1, "description": "CTD_Raw PRES" },
      { "id": "9A", "bytes":   28, "packets":  1, "description": "CTD_Drift PRES" },
      { "id": "A0", "bytes":   70, "packets":  1, "description": "CTD_Raw_Descend TEMP" },
      { "id": "A5", "bytes":   83, "packets":  1, "description": "CTD_Raw TEMP" },
      { "id": "AA", "bytes":   28, "packets":  1, "description": "CTD_Drift TEMP" },
      { "id": "B0", "bytes":   70, "packets":  1, "description": "CTD_Raw_Descend PSAL" },
      { "id": "B5", "bytes":   83, "packets":  1, "description": "CTD_Raw PSAL" },
      { "id": "BA", "bytes":   27, "packets":  1, "description": "CTD_Drift PSAL" },
      { "id": "DA", "bytes":   28, "packets":  1, "description": "CTD_Drift absolute_pressure" },
      { "id": "DE", "bytes":   46, "packets":  1, "description": "Mission Upload Command" },
      { "id": "E2", "bytes":  106, "packets":  1, "description": "Engineering parameters" },
      { "id": "F0", "bytes":   41, "packets":  1, "description": "Argo Mission" },
      { "id": "F1", "bytes":   22, "packets":  1, "description": "Engineering SBEerr" }
    ]
  },
}
```

<h3>GPS</h3>
<p>The GPS section includes GPS fixes received during the current cycle. DS floats typically transmit two GPS fixes during each cycle. <b>GPS_START</b> refers to the gps fix prior to descent and <b>GPS_END</b> refers to the gps fix after surfacing.</p>

```javascript
{
  "GPS": [
    { "description": "GPS_START", "TIME": "2025-02-24T09:23:00Z", "LATITUDE":  41.53426, "LONGITUDE":  -70.64682, "HDOP":   0.9, "sat_cnt":  9, "snr_min": 19, "snr_mean": 34, "snr_max": 48, "time_to_fix": 20, "valid": -2 },
    { "description":   "GPS_END", "TIME": "2025-02-24T22:21:00Z", "LATITUDE":  41.53427, "LONGITUDE":  -70.64680, "HDOP":   0.9, "sat_cnt":  9, "snr_min": 32, "snr_mean": 38, "snr_max": 46, "time_to_fix": 20, "valid": -2 }
  ]
}
```

<h3>ARGO_Mission</h3>
<p>The ARGO Mission packet summarizes a subset of the current float CONFIG used in the present cycle. It includes firmware version information, profile targets and durations, as well as the CTD gains and offsets. This packet may not be transmitted every cycle.</p>

```javascript
{
  "ARGO_Mission": {
    "float_model": 1,
    "float_telemetry_format": 2.2,
    "min_ascent_rate_cmpersec": 12,
    "profile_target_dbar": 1000,
    "drift_target_dbar": 600,
    "max_rise_minute":  300,
    "max_fall_to_profile_minute":  300,
    "target_drift_5minute":  48,
    "target_surface_second": 2700,
    "seek_periods": 4,
    "seek_minute": 120,
    "TurnAround_timeTurn":  240,
    "TurnAround_timeCheck":   60,
    "ctd_pres": { "gain":   10, "offset":  10},
    "ctd_temp": { "gain": 1000, "offset":   5},
    "ctd_psal": { "gain": 1000, "offset":   1}
  }
}
```

<h3>Fall and Rise</h3>
<p>DS floats transmit a pressure time-series during fall and another one during rise. Timestamps and phase information is included for each pressure scan [dbar]</p>

```javascript
{
  "Fall": [
    { "TIME": "2026-04-26T23:00:41Z", "PRES":    -0.2, "phase":  1, "description": "Start of sink" },
    { "TIME": "2026-04-26T23:03:11Z", "PRES":     0.4, "phase": 10, "description": "Sinking" },
    { "TIME": "2026-04-26T23:08:11Z", "PRES":    65.9, "phase":  2, "description": "Pump 2 target" },
    { "TIME": "2026-04-27T01:21:51Z", "PRES":   868.4, "phase": 10, "description": "Sinking" },
    { "TIME": "2026-04-27T01:52:41Z", "PRES":  1001.8, "phase":  9, "description": "End of Sink" },
    { "TIME": "2026-04-27T02:53:21Z", "PRES":   808.0, "phase":  6, "description": "Turnaround" },
    { "TIME": "2026-04-27T05:53:41Z", "PRES":   615.5, "phase":  3, "description": "Seek" },
    { "TIME": "2026-04-27T13:55:01Z", "PRES":   601.0, "phase":  4, "description": "Drift begin" }
  ],
  "Rise": [
    { "TIME": "2026-04-27T17:55:20Z", "PRES":   613.3, "phase":  7, "description": "Profile start" },
    { "TIME": "2026-04-27T18:54:40Z", "PRES":   172.6, "phase": 13, "description": "Ascending" },
    { "TIME": "2026-04-27T19:15:20Z", "PRES":     1.2, "phase":  8, "description": "Profile end" },
    { "TIME": "2026-04-27T19:22:50Z", "PRES":     0.6, "phase": 14, "description": "Reached surface" }
  ]
}
```

<h3>Buoyancy Pump</h3>
<p>DS floats transmit a buoyancy pump record. The measured pressure and phase closely matches the same fields in the Rise/Fall (but may not always match)</p>

```javascript
{
  "Pump": [
    { "PRES":   65.9, "elapsed_minutes":      7, "current":  485, "voltage": 14.85, "pump_time":  288, "vac_start":   3, "vac_end":   5, "phase":  2, "description": "Pump 2 target" },
    { "PRES": 1002.7, "elapsed_minutes":    172, "current": 1113, "voltage": 14.88, "pump_time":   73, "vac_start":   3, "vac_end":  19, "phase":  6, "description": "Turnaround" },
    { "PRES":  671.2, "elapsed_minutes":    292, "current": 1025, "voltage": 15.02, "pump_time":    1, "vac_start":   3, "vac_end":  21, "phase":  6, "description": "Turnaround" },
    { "PRES":  625.8, "elapsed_minutes":    773, "current": 1015, "voltage": 15.10, "pump_time":    1, "vac_start":   3, "vac_end":  22, "phase":  3, "description": "Seek" },
    { "PRES":  613.3, "elapsed_minutes":   1134, "current":  954, "voltage": 15.10, "pump_time":   80, "vac_start":   3, "vac_end":  28, "phase":  7, "description": "Profile start" },
    { "PRES":  586.9, "elapsed_minutes":   1140, "current":  912, "voltage": 15.02, "pump_time":   80, "vac_start":   3, "vac_end":  28, "phase": 13, "description": "Ascending" },
    { "PRES":  112.2, "elapsed_minutes":   1202, "current":  587, "voltage": 14.99, "pump_time":   80, "vac_start":   3, "vac_end":  20, "phase": 13, "description": "Ascending" },
    { "PRES":    0.6, "elapsed_minutes":   1222, "current":  465, "voltage": 14.94, "pump_time":  223, "vac_start":   3, "vac_end": 106, "phase": 14, "description": "Reached surface" }
  ]
}
```

<h3>Engineering</h3>
<p>DS floats transmit a various float Engineering values to track float health. "Eng_ver" is used to identify the correct engineering format in the /config directory.</p>

```javascript
{
  "Engineering_Data": {
     "Eng_ver": { "value":        7, "unit":    "1", "description": "Engineering Packet software version" },
     "nQueued": { "value":       12, "unit":    "1", "description": "# of data blocks queued for this dive" },
      "nTries": { "value":       14, "unit":    "1", "description": "# of tries to get the data block receive status in previous dive" },
    "parXstat": { "value":        0, "unit":    "1", "description": "parse_x_reply status in the last surface session" },
    "SBDIstat": { "value":        1, "unit":    "1", "description": "ATSBD return status in last surface session" },
     "SBDsecs": { "value":      207, "unit":    "s", "description": "time taken in sending last SBD message" },
        "Vcpu": { "value":     7.02, "unit":    "V", "description": "CPU battery voltage" },
        "Vpmp": { "value":    15.04, "unit":    "V", "description": "present Pump battery voltage" },
        "Vple": { "value":    14.94, "unit":    "V", "description": "pump battery voltage at end of last pump" },
      "vac50m": { "value":    10.32, "unit": "inHg", "description": "pcase vacuum @50m descent" },
        "vac0": { "value":    10.85, "unit": "inHg", "description": "pcase vacuum before filling oil bladder at surface" },
        "vac1": { "value":    10.99, "unit": "inHg", "description": "pcase vacuum after filling oil bladder at surface" },
  }
}
```

<h3>Profile Data ("CTD_Raw", "CTD_Raw_Descend", "CTD_Binned", "CTD_Binned_Descend", "CTD_Discrete", CTD_Discrete_Descend", "CTD_Drift")</h3>
<p>DS floats transmit 7 types of scientific CTD sensor profiles [1 during drift, 3 on ascent and 3 on descent]</p>

```javascript
{
  "CTD_Binned": [
    { "PRES":   903.8, "TEMP":  4.248, "PSAL":  34.457 },
    { "PRES":   902.2, "TEMP":  4.250, "PSAL":  34.456 },
    { "PRES":   900.1, "TEMP":  4.257, "PSAL":  34.456 },
    { "PRES":   897.8, "TEMP":  4.265, "PSAL":  34.455 },
    { "PRES":   895.8, "TEMP":  4.271, "PSAL":  34.454 },
    { "PRES":   893.9, "TEMP":  4.273, "PSAL":  34.452 },
    { "PRES":   892.0, "TEMP":  4.278, "PSAL":  34.452 },
    { "PRES":   890.0, "TEMP":  4.289, "PSAL":  34.449 },
    { "PRES":   887.7, "TEMP":  4.291, "PSAL":  34.446 },
    { "PRES":   885.9, "TEMP":  4.289, "PSAL":  34.445 },
    { "PRES":   884.0, "TEMP":  4.290, "PSAL":  34.444 },
  ]
  "CTD_Discrete_Descend": [
    { "elapsed_second":       0, "PRES":  2004.3, "TEMP":  2.154, "PSAL":  34.620 },
    { "elapsed_second":      48, "PRES":  2011.5, "TEMP":  2.151, "PSAL":  34.620 },
    { "elapsed_second":     114, "PRES":  2021.5, "TEMP":  2.136, "PSAL":  34.621 },
    { "elapsed_second":     180, "PRES":  2031.2, "TEMP":  2.130, "PSAL":  34.622 },
    { "elapsed_second":     249, "PRES":  2041.5, "TEMP":  2.118, "PSAL":  34.623 },
    { "elapsed_second":     317, "PRES":  2051.4, "TEMP":  2.111, "PSAL":  34.624 },
    { "elapsed_second":     385, "PRES":  2061.5, "TEMP":  2.106, "PSAL":  34.624 },
    { "elapsed_second":     455, "PRES":  2071.7, "TEMP":  2.097, "PSAL":  34.624 },
    { "elapsed_second":     523, "PRES":  2081.6, "TEMP":  2.093, "PSAL":  34.625 },
    { "elapsed_second":     592, "PRES":  2091.7, "TEMP":  2.082, "PSAL":  34.625 },
  ]
}
```
