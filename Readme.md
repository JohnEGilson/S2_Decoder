<h1>S2 Decoder</h1>
<p>S2 Decoder is a free and open-source decoder (GPL-3.0) to convert raw SOLO-II telemetry into human readable .json files</p>

<h2>Acknowledgements</h2>
<p>S2_Decoder code uses an existing codebase as a foundational starting point, modifying it to be used with a related Argo float model.</p> 
<p>[Original Project Name]: https://github.com/Greenwood1981/S2BGC_Decoder</p>

<h2>Requirements</h2>
<ul>
  <li>A Linux operating system is recommended</li>
  <li>Compile with GNU g++ v11.4 or newer</li>
  <li>c++ boost libraries</li>
  <li>The <b>S2_PATH</b> environmental variable must be defined prior to running the decoder.</li>
  <li>Modify config/default_meta.json to match your institution's information</li>
</ul>

<h2>Installation</h2>
<p>To install, clone this repo, and run make. g++ v11.4 or newer is recommended</p>

<h2>Operation</h2>
<p>S2_Decoder can be automated using a cronjob or run manually by a user from the command line.</p>

<h2>Cronjob Example</h2>
<pre>
S2_PATH="/path/S2_Decoder"
#  min hr dom mon dow command
  */15  *   *   *   * /path/S2_Decoder 2>&1
</pre>

<h2>Code summary</h2>

<h2>File Structure</h2>
<pre>
S2_Decoder/
├─ config/
│  ├─ config.json         (User configurable parameters - path definitions, etc..)
│  ├─ default_meta.json   (Default meta info assigned to a float)
│  ├─ mission.json        (S2 Mission json schema)
│  ├─ Engineering_Data_5(6).json  (S2 Engineering data versions)
│  ├─ diagnostic_5(6).json        (S2 Engineering diagnostic dive data versions)
├─ data/
│  ├─ 3383/ (Following the example data provided)
│  │  ├─ 3383_meta.json   (float specific meta info added near the top of each .json file)
│  │  ├─ hex
│  │  │  ├─ 3383_0000.hex  (hex files are placed in this subdirectory after being processed)
│  │  │  ├─ 3383_0001.hex
│  │  ├─ json
│  │  │  ├─ L0
│  │  │  |    3383_L0_0000.json (decoded .json file)
│  │  │  |    3383_L0_0001.json (decoded .json file)
│  │  │  ├─ SBD
│  │  │  |    3383_SBD_0000.json (decoded .json file created from SBD_S2.cpp)
│  │  │  |    3383_SBD_0001.json (decoded .json file created from SBD_S2.cpp)
│  │  │  ├─ L1
│  │  │  |    3383_L1_0000.json (decoded .json file created from L0toL1_S2.cpp)
│  │  │  |    3383_L1_0001.json (decoded .json file created from L0toL1_S2.cpp)
│  ├─ 33xx/
│  │  ├─ 33xx_meta.json
│  │  ├─ hex
│  │  ├─ json
├─ incoming/
│  ├─ 3383_0002.hex        (unprocessed hex files)
│  ├─ 3383_0003.hex
├─ log/                   (daily log files are generated and placed in this subdirectory)
├─ src/                   (Decoder c++ source code)
</pre>

<h2>Hex files</h2>
<p>S2 raw telemetry must first be converted into a .hex file before it is decoded. A single .hex file is created for each float cycle. The .hex file is capable of formatting data from SBD or RUDICS telemetry. All S2 messages that belong to the same cycle are concatenated together and are ordered by SBD message ID.</p>
<p>Each message includes two header lines that include additional information about the SBD message or RUDICS session. Note the Iridium SBD doppler position in the header.</p>
<p>Following the header lines is the raw binary data, represented as two-digit zero padded hex values. Up to 32 bytes of data is included on each line</p>
<h3>Hex file example</h3>
<pre>
#          SN Cycle  Size IMEI   MOMSN MTMSN PID                  Date         Lat         Lon   Err
SBD,     3383,    6,  267,   0,     81,    0,  1, 2026,03,13, 07,09,02,  -48.59324,  164.29298,    3
58 01 04 0d 37 00 06 01 b8 10 54 00 00 3d 00 8a 3e 00 00 05 49 20 00 00 00 00 00 00 00 00 00 00
fa 07 f4 05 02 01 ff fe f9 18 e1 14 f5 0e ff f1 0d f8 02 fd fe 0f f2 fd fd 18 f8 ee 1b f1 f7 0c
ff f9 08 14 e4 f3 22 fb e4 0e 12 f9 ee 0a 07 ef 15 f9 f4 05 20 ea d9 25 15 d2 1b 3b 02 00 18 02
e3 0c c5 18 61 ec 78 8a 09 69 05 07 06 13 05 27 2c 31 0c 3b 23 10 58 38 00 70 00 1d cb ff ff ff
44 92 48 00 00 00 00 00 00 00 00 00 00 ff 02 ff 00 01 ff ff 00 00 01 ff f7 05 05 00 f2 f0 d1 03
f0 01 df 1e 22 1e 1b 23 01 0f f1 00 d1 11 0f c4 1e 4e ff 20 1f fb 14 00 10 1f 00 1f 1e 1f 1f 1c
e5 10 2f e2 fe 2f 0f 10 01 f2 d0 3b 60 10 3b 20 0b 42 00 1f 05 d4 01 9c 02 03 a0 66 3f 00 02 05
cd 06 b6 02 03 70 c5 0f 00 4a 05 8c 0b 3f 02 06 d0 47 14 00 12 05 be 04 db 02 09 80 00 fa 00 a5
05 db 01 04 02 0d 3b 24 32 33 3e
...
</pre>
<h2>JSON output</h2>
<p>The S2 decoder is designed to output a single file for each float cycle. Any sections missing in the .json file should be interpreted as packets that have not yet been received. Sample .json files are provided.</a></p>

<h3>Float Specific metadata</h3>
<p>Each S2 .json file begins with a collection of float specific metadata. This information is meant to be updated by the user and is placed in a file in a float's subdirectory <b>data/3xxx/3xxx_meta.json</b>. Default meta information is assigned each time a new float is processed. The default values may be updated in <b>config/default_meta.json</b>.</p>

```javascript
{
  "FILE_CREATION_DATE": "2025-05-19T12:20:01Z",
  "DECODER_VERSION": "0.87",
  "SCHEMA_VERSION": "0.1",
  "INTERNAL ID NUMBER": 4008,
  "DAC_ID NUMBER": 99999,
  "WMO_ID NUMBER": 9999999,
  "TRANSMISSION ID NUMBER": 9999,
  "INSTRUMENT TYPE": "SOLO_II",
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
    { "PID":  0, "source": "SBD", "TIME": "2026-04-02T00:58:28Z", "momsn": 117, "size":  301, "sensor_ids": [240,1,152,168] },
    { "PID":  1, "source": "SBD", "TIME": "2026-04-02T00:58:34Z", "momsn": 118, "size":  337, "sensor_ids": [184,2,32,96,160] },
    { "PID":  2, "source": "SBD", "TIME": "2026-04-02T00:59:00Z", "momsn": 119, "size":  301, "sensor_ids": [16] },
    { "PID":  3, "source": "SBD", "TIME": "2026-04-02T00:59:08Z", "momsn": 120, "size":  289, "sensor_ids": [16] },
    { "PID":  4, "source": "SBD", "TIME": "2026-04-02T00:59:19Z", "momsn": 121, "size":  325, "sensor_ids": [32] },
    { "PID":  5, "source": "SBD", "TIME": "2026-04-02T00:59:45Z", "momsn": 122, "size":  325, "sensor_ids": [32] },
    { "PID":  6, "source": "SBD", "TIME": "2026-04-02T01:00:00Z", "momsn": 123, "size":  333, "sensor_ids": [32] },
    { "PID":  7, "source": "SBD", "TIME": "2026-04-02T01:00:08Z", "momsn": 124, "size":  333, "sensor_ids": [48] },
    { "PID":  8, "source": "SBD", "TIME": "2026-04-02T01:00:16Z", "momsn": 125, "size":  336, "sensor_ids": [48,48] },
    { "PID":  9, "source": "SBD", "TIME": "2026-04-02T01:00:28Z", "momsn": 126, "size":  323, "sensor_ids": [64,80] },
    { "PID": 10, "source": "SBD", "TIME": "2026-04-02T01:00:36Z", "momsn": 127, "size":  293, "sensor_ids": [144] },
    { "PID": 11, "source": "SBD", "TIME": "2026-04-02T01:00:53Z", "momsn": 128, "size":  280, "sensor_ids": [144] },
    { "PID": 12, "source": "SBD", "TIME": "2026-04-02T01:01:04Z", "momsn": 129, "size":  325, "sensor_ids": [160] },
    { "PID": 13, "source": "SBD", "TIME": "2026-04-02T01:01:28Z", "momsn": 130, "size":  333, "sensor_ids": [160] },
    { "PID": 14, "source": "SBD", "TIME": "2026-04-02T01:02:52Z", "momsn": 131, "size":  325, "sensor_ids": [176] },
    { "PID": 15, "source": "SBD", "TIME": "2026-04-02T01:04:14Z", "momsn": 132, "size":  333, "sensor_ids": [176] },
    { "PID": 16, "source": "SBD", "TIME": "2026-04-02T01:04:30Z", "momsn": 133, "size":  247, "sensor_ids": [176,226] }
  ]
}
```

<h3>packet_info</h3>
<p>Each S2 session may contain multiple packets of information. This section highlights each type of packets that have been received in a human readable format</p>

```javascript
{
 "packet_info": {
    "packet_count": 27,
    "packet_bytes": 5135,
    "packet_type": [
      { "id": "01", "bytes":   24, "packets":  1, "description": "GPS start-dive" },
      { "id": "02", "bytes":   24, "packets":  1, "description": "GPS end-dive" },
      { "id": "10", "bytes":  566, "packets":  2, "description": "CTD_Binned PRES" },
      { "id": "20", "bytes": 1005, "packets":  4, "description": "CTD_Binned TEMP" },
      { "id": "30", "bytes":  645, "packets":  3, "description": "CTD_Binned PSAL" },
      { "id": "40", "bytes":  173, "packets":  1, "description": "Engineering Fall" },
      { "id": "50", "bytes":  138, "packets":  1, "description": "Engineering Rise" },
      { "id": "60", "bytes":   92, "packets":  1, "description": "Engineering Pump" },
      { "id": "90", "bytes":  549, "packets":  2, "description": "CTD_Raw PRES" },
      { "id": "98", "bytes":  114, "packets":  1, "description": "CTD_Drift PRES" },
      { "id": "A0", "bytes":  701, "packets":  3, "description": "CTD_Raw TEMP" },
      { "id": "A8", "bytes":  114, "packets":  1, "description": "CTD_Drift TEMP" },
      { "id": "B0", "bytes":  765, "packets":  3, "description": "CTD_Raw PSAL" },
      { "id": "B8", "bytes":   84, "packets":  1, "description": "CTD_Drift PSAL" },
      { "id": "E2", "bytes":  104, "packets":  1, "description": "Engineering parameters" },
      { "id": "F0", "bytes":   37, "packets":  1, "description": "Argo Mission" }
    ]
  },
}
```

<h3>GPS</h3>
<p>The GPS section includes GPS fixes received during the current cycle. S2 floats typically transmit two GPS fixes during each cycle. <b>GPS_START</b> refers to the gps fix prior to descent and <b>GPS_END</b> refers to the gps fix after surfacing.</p>

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
   "float_model": 0,
    "float_telemetry_format": 2.6,
    "min_ascent_rate_cmpersec": 13,
    "profile_target_dbar": 2000,
    "drift_target_dbar": 1000,
    "max_rise_minute":  600,
    "max_fall_to_park_minute":  400,
    "max_fall_to_profile_minute":  330,
    "target_drift_5minute": 2640,
    "target_surface_second": 2145,
    "seek_periods": 2,
    "seek_minute": 120,
    "ctd_pres": { "gain":   25, "offset":  10},
    "ctd_temp": { "gain": 1000, "offset":   5},
    "ctd_psal": { "gain": 1000, "offset":   1}
  }
}
```

<h3>Fall and Rise</h3>
<p>S2 floats transmit a pressure time-series during fall and another one during rise. Timestamps and phase information is included for each pressure scan [dbar]</p>

```javascript
{
  "Fall": [
    { "TIME": "2026-03-23T04:14:08Z", "PRES":   -0.12, "phase":  1, "description": "Start of sink" },
    { "TIME": "2026-03-23T04:15:48Z", "PRES":   12.12, "phase": 10, "description": "Sinking" },
    { "TIME": "2026-03-23T04:24:08Z", "PRES":  105.00, "phase":  2, "description": "Pump 2 target" },
    { "TIME": "2026-03-23T09:14:08Z", "PRES":  944.76, "phase": 10, "description": "Sinking" },
    { "TIME": "2026-03-23T12:59:50Z", "PRES": 1007.28, "phase":  3, "description": "Seek" },
    { "TIME": "2026-03-23T15:00:14Z", "PRES": 1003.92, "phase":  4, "description": "Drift begin" }
  ],
  "Rise": [
    { "TIME": "2026-04-01T19:00:37Z", "PRES": 1027.56, "phase":  6, "description": "Fall to profile begin" },
    { "TIME": "2026-04-01T19:20:37Z", "PRES": 1185.24, "phase": 12, "description": "Descending" },
    { "TIME": "2026-04-01T21:34:17Z", "PRES": 2012.76, "phase":  7, "description": "Profile start" },
    { "TIME": "2026-04-02T00:45:11Z", "PRES":   17.36, "phase": 13, "description": "Ascending" },
    { "TIME": "2026-04-02T00:47:32Z", "PRES":    0.16, "phase":  8, "description": "Profile end" },
    { "TIME": "2026-04-02T00:56:38Z", "PRES":   -0.08, "phase": 14, "description": "Reached surface" }
  ]
}
```

<h3>Buoyancy Pump</h3>
<p>S2 floats transmit a buoyancy pump record. The measured pressure and phase closely matches the same fields in the Rise/Fall (but may not always match)</p>

```javascript
{
  "Pump": [
    { "PRES":  105.00, "current":  413, "voltage": 14.91, "pump_time":   33, "vac_start":   3, "vac_end":   3, "phase":  2, "description": "Pump 2 target" },
    { "PRES": 1018.00, "current": 1688, "voltage": 14.84, "pump_time":    3, "vac_start":   2, "vac_end":   3, "phase": 10, "description": "Sinking" },
    { "PRES":  920.88, "current":    2, "voltage": 15.46, "pump_time":   -2, "vac_start":   2, "vac_end":   3, "phase":  3, "description": "Seek" },
    { "PRES": 2012.76, "current": 2887, "voltage": 14.19, "pump_time":   75, "vac_start":   2, "vac_end":   5, "phase":  7, "description": "Profile start" },
    { "PRES":  706.20, "current": 1232, "voltage": 14.71, "pump_time":   18, "vac_start":   2, "vac_end":   9, "phase": 13, "description": "Ascending" },
    { "PRES":   52.64, "current":  349, "voltage": 15.00, "pump_time":   18, "vac_start":   2, "vac_end":   8, "phase": 13, "description": "Ascending" },
    { "PRES":    0.08, "current":  260, "voltage": 14.98, "pump_time":  165, "vac_start":   3, "vac_end":  20, "phase":  8, "description": "Profile end" },
    { "PRES":    0.08, "current":  257, "voltage": 14.94, "pump_time":   75, "vac_start":   3, "vac_end":  27, "phase": 14, "description": "Reached surface" }
  ]
}
```

<h3>Engineering</h3>
<p>S2 floats transmit a various float Engineering values to track float health. "Eng_ver" is used to identify the correct engineering format in the /config directory.</p>

```javascript
{
  "Engineering_Data": {
       "Eng_ver": { "value":        6, "unit":     "", "description": "Engineering Packet software version" },
       "nQueued": { "value":       17, "unit":     "", "description": "# of data blocks queued for this dive" },
        "nTries": { "value":       19, "unit":     "", "description": "# of tries to get the data block receive status in previous dive" },
      "parXstat": { "value":        0, "unit":     "", "description": "parse_x_reply status in the last surface session" },
      "SBDIstat": { "value":        1, "unit":     "", "description": "ATSBD return status in last surface session" },
       "SBDsecs": { "value":      261, "unit":    "s", "description": "time taken in sending last SBD message" },
          "Vcpu": { "value":     7.06, "unit":    "V", "description": "CPU battery voltage" },
          "Vpmp": { "value":    15.16, "unit":    "V", "description": "present Pump battery voltage" },
          "Vple": { "value":    14.94, "unit":    "V", "description": "pump battery voltage at end of last pump" },
        "vac50m": { "value":     9.78, "unit": "inHg", "description": "pcase vacuum @50m descent" },
          "vac0": { "value":    10.23, "unit": "inHg", "description": "pcase vacuum before filling oil bladder at surface" },
          "vac1": { "value":    11.06, "unit": "inHg", "description": "pcase vacuum after filling oil bladder at surface" }
  }
}
```

<h3>Profile Data ("CTD_Raw", "CTD_Binned", "CTD_Drift")</h3>
<p>S2 floats transmit 3 types of scientific CTD sensor profiles.</p>

```javascript
{
  "CTD_Binned": [
    { "PRES":    6.88, "TEMP": 12.141, "PSAL": 34.675 },
    { "PRES":    6.00, "TEMP": 12.141, "PSAL": 34.675 },
    { "PRES":    5.04, "TEMP": 12.142, "PSAL": 34.675 },
    { "PRES":    3.92, "TEMP": 12.141, "PSAL": 34.675 },
    { "PRES":    2.96, "TEMP": 12.141, "PSAL": 34.675 },
    { "PRES":    2.00, "TEMP": 12.142, "PSAL": 34.675 },
    { "PRES":    1.00, "TEMP": 12.140, "PSAL": 34.675 },
    { "PRES":    0.48, "TEMP": 12.142, "PSAL": 34.674 }
  ]
}
```
