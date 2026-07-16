# PC Screen Stream Protocol v1

The PC sends one 320x240 RGB332 frame as 60 UDP datagrams. Each frame-data
datagram carries four complete scan lines (1280 bytes), so the UDP payload is
1292 bytes and does not require IP fragmentation on a normal Ethernet/Wi-Fi MTU.

## Header

All multi-byte fields use network byte order. The header is exactly 12 bytes.

| Offset | Size | Field | Value |
| --- | ---: | --- | --- |
| 0 | 2 | magic | ASCII `SS` |
| 2 | 1 | protocol_version | `1` |
| 3 | 1 | packet_type | `1` frame, `2` heartbeat, `3` reserved control |
| 4 | 2 | frame_id | Unsigned 16-bit counter with wraparound |
| 6 | 2 | y_start | First destination scan line |
| 8 | 1 | line_count | `4` for frame data |
| 9 | 1 | color_mode | `1` RGB332 |
| 10 | 2 | payload_length | `1280` for frame data, `0` for heartbeat |

RGB332 uses bits `RRRGGGBB`. Packets with an invalid magic, version, length,
color mode, line count, or scan-line boundary are discarded.

The receiver keeps a 60-bit chunk map. A frame is submitted to the LCD only
after all bits are present. An incomplete frame is discarded when a newer
`frame_id` arrives or 100 ms after its first packet. The previous complete LCD
frame remains visible.
