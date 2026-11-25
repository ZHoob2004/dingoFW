import cantools
from utils.network_keep_awake import build_network_keep_awake


def build_db(base_id, version):
    db = cantools.database.Database()
    db.name = "LINBoard"
    db.version = version
    db.messages.append(cantools.database.Message(
        frame_id=base_id + 2,
        name="LINBoardStatus",
        length=8,
        is_extended_frame=False,
        cycle_time=100,
        signals=[
            cantools.database.Signal("DeviceState", 0, 8, byte_order="little_endian", is_signed=False),
            cantools.database.Signal("Heartbeat", 56, 8, byte_order="little_endian", is_signed=False),
        ],
    ))
    db.messages.append(build_network_keep_awake(base_id, 3))
    return db
