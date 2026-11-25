import cantools


def build_network_keep_awake(base_id, offset):
    """Version 1 network-management request, emitted only while local demand exists."""
    fields = [
        ("ProtocolSignature", 0, 24),
        ("ProtocolVersion", 24, 8),
        ("SenderBaseId", 32, 16),
        ("NetworkKeepAwake", 48, 1),
        ("ReservedFlags", 49, 7),
        ("Sequence", 56, 8),
    ]
    return cantools.database.Message(
        frame_id=base_id + offset,
        name="NetworkKeepAwake",
        length=8,
        is_extended_frame=False,
        comment="DNG (0x474E44), version 1, little-endian base ID. "
                "Flags bit 0 requests keep-awake. Reserved flags are zero. "
                "Sent every 100 ms only while local demand exists.",
        signals=[
            cantools.database.Signal(name=name, start=start, length=length,
                                     byte_order="little_endian", is_signed=False)
            for name, start, length in fields
        ],
    )


def build_network_db(name, version, base_id, offset):
    """Network-message-only DBC for boards without a full cyclic DBC builder."""
    db = cantools.database.Database()
    db.name = name
    db.version = version
    db.messages.append(build_network_keep_awake(base_id, offset))
    return db
