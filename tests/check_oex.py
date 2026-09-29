"""Validate packaging of the Android ARM64 UCI executable in the OEX APK."""
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree

apk = Path(sys.argv[1])
with zipfile.ZipFile(apk) as bundle:
    binary = bundle.read('lib/arm64-v8a/libcce.so')
    assert binary[:4] == b'\x7fELF', 'missing ELF header'
    assert binary[4] == 2 and binary[5] == 1, 'expected 64-bit little endian'
    assert int.from_bytes(binary[18:20], 'little') == 183, 'expected AArch64'
    xml = ElementTree.fromstring(bundle.read('assets/enginelist.xml'))
    assert any(e.get('filename') == 'libcce.so' and
               e.get('target') == 'arm64-v8a' for e in xml.iter('engine'))
    assert bundle.getinfo('lib/arm64-v8a/libcce.so').file_size > 10000
print('OEX APK package check passed:', apk)
