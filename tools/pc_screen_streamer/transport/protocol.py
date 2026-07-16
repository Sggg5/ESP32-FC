import struct
from dataclasses import dataclass


MAGIC = b"SS"
PROTOCOL_VERSION = 1
PACKET_FRAME_DATA = 1
PACKET_HEARTBEAT = 2
PACKET_CONTROL = 3
COLOR_RGB332 = 1
HEADER = struct.Struct("!2sBBHHBBH")
HEADER_SIZE = HEADER.size


@dataclass(frozen=True)
class PacketHeader:
    packet_type: int
    frame_id: int
    y_start: int
    line_count: int
    color_mode: int
    payload_length: int

    def pack(self) -> bytes:
        return HEADER.pack(
            MAGIC,
            PROTOCOL_VERSION,
            self.packet_type,
            self.frame_id & 0xFFFF,
            self.y_start,
            self.line_count,
            self.color_mode,
            self.payload_length,
        )


def frame_packet(frame_id: int, y_start: int, line_count: int, payload: bytes) -> bytes:
    header = PacketHeader(
        packet_type=PACKET_FRAME_DATA,
        frame_id=frame_id,
        y_start=y_start,
        line_count=line_count,
        color_mode=COLOR_RGB332,
        payload_length=len(payload),
    )
    return header.pack() + payload


def heartbeat_packet(frame_id: int) -> bytes:
    return PacketHeader(PACKET_HEARTBEAT, frame_id, 0, 0, COLOR_RGB332, 0).pack()
