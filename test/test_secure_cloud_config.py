"""Opt-in is coupled to the reviewed transport; ordinary targets stay supported."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cases = [
    ('config Linux normal', 'SecureCloudConfig.h', [], True, ''),
    ('config ESP32 normal', 'SecureCloudConfig.h', ['ARDUINO', 'ESP32'], True, ''),
    ('config supported', 'SecureCloudConfig.h', ['ARDUINO', 'ESP8266', 'OG_SECURE_CLOUD'], True, ''),
    ('config Linux secure', 'SecureCloudConfig.h', ['OG_SECURE_CLOUD'], False, 'reviewed Arduino ESP8266'),
    ('config ESP32 secure', 'SecureCloudConfig.h', ['ARDUINO', 'ESP32', 'OG_SECURE_CLOUD'], False, 'reviewed Arduino ESP8266'),
    ('transport reviewed', 'Websocket.h', ['ARDUINO', 'ESP8266', 'OG_SECURE_CLOUD'], True, ''),
    ('transport missing capability', 'Websocket.h', ['ARDUINO', 'ESP8266', 'OG_SECURE_CLOUD', 'TEST_UNVERIFIED_TRANSPORT'], False, 'reviewed verified WebSockets'),
    ('transport wrong capability', 'Websocket.h', ['ARDUINO', 'ESP8266', 'OG_SECURE_CLOUD', 'OG_VERIFIED_WSS_TRANSPORT=2'], False, 'reviewed verified WebSockets'),
    ('transport ordinary ESP8266', 'Websocket.h', ['ARDUINO', 'ESP8266', 'TEST_UNVERIFIED_TRANSPORT'], True, ''),
    ('transport ordinary ESP32', 'Websocket.h', ['ARDUINO', 'ESP32', 'TEST_UNVERIFIED_TRANSPORT'], True, ''),
]
with tempfile.TemporaryDirectory() as td:
    p = Path(td) / 'test.cpp'
    for label, header, flags, ok, error in cases:
        p.write_text('#include "'+header+'"\n')
        r = subprocess.run(['g++', '-std=c++14', '-fsyntax-only', '-I'+str(root),
                            '-I'+str(root/'test/fakes/reconnect'), '-I'+str(root/'test/fakes'),
                            *['-D'+flag for flag in flags], str(p)], capture_output=True, text=True)
        assert (r.returncode == 0) == ok, label+'\n'+r.stderr
        if error:
            assert error in r.stderr, label+'\n'+r.stderr
print('PASS secure opt-in platform/capability checks and ordinary ESP8266/ESP32/Linux builds')
