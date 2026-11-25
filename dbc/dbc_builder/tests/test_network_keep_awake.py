import unittest

import canboard
import dingopdm
import dingopdm_max
from utils.network_keep_awake import build_network_db


class NetworkKeepAwakeTest(unittest.TestCase):
    def test_device_builders(self):
        cases = [
            (canboard.build_db(0x640), 0x640, 15),
            (dingopdm.build_db(0x0DE), 0x0DE, 55),
            (dingopdm_max.build_db(0x0DE), 0x0DE, 53),
            (build_network_db("PT-DPDM", "test", 0x0DE, 54), 0x0DE, 54),
            (build_network_db("dingoPdm-v8", "test", 0x0DE, 39), 0x0DE, 39),
        ]
        for db, base_id, offset in cases:
            with self.subTest(device=db.name):
                db.refresh()
                ids = [msg.frame_id for msg in db.messages]
                self.assertEqual(len(set(ids)), len(ids))
                msg = db.get_message_by_name("NetworkKeepAwake")
                self.assertEqual(msg.frame_id, base_id + offset)
                self.assertEqual(msg.length, 8)
                self.assertFalse(msg.is_extended_frame)
                values = {
                    "ProtocolSignature": 0x474E44,
                    "ProtocolVersion": 1,
                    "SenderBaseId": base_id,
                    "NetworkKeepAwake": 1,
                    "ReservedFlags": 0,
                    "Sequence": 0xA5,
                }
                payload = bytes([0x44, 0x4E, 0x47, 1, base_id & 0xFF,
                                 base_id >> 8, 1, 0xA5])
                self.assertEqual(msg.encode(values), payload)
                self.assertEqual(msg.decode(payload), values)


if __name__ == "__main__":
    unittest.main()
