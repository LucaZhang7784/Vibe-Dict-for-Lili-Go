"""Host checks against real SD data and corrupt fixtures. Run inside IDF image."""
import ctypes as c
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run(["gcc", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror", "-o", "/tmp/dict_core.so", str(root / "main/dict_core.c")], check=True)
lib = c.CDLL("/tmp/dict_core.so")
class Entry(c.Structure):
    _fields_ = [("head", c.c_char * 128), ("phonetic", c.c_char * 256), ("definition", c.c_char * 16384)]
class Candidate(c.Structure):
    _fields_ = [("head", c.c_char * 64)]
lib.dict_lookup.argtypes = [c.c_char_p,c.c_bool,c.c_char_p,c.c_uint,c.POINTER(Entry),c.POINTER(c.c_uint)]
lib.dict_candidates.argtypes = [c.c_char_p,c.c_char_p,c.POINTER(c.c_uint32),c.POINTER(Candidate),c.POINTER(c.c_bool)]
lib.dict_audio_name.argtypes = [c.c_char_p,c.c_char_p,c.c_size_t]
lib.dict_audio_name.restype = c.c_bool
lib.dict_wav_open.argtypes = [c.c_char_p,c.POINTER(c.c_void_p),c.POINTER(c.c_uint32)]
base = Path(sys.argv[1])
sys.path.insert(0,str(base.parents[1] / "apps/lib"))
from local_dict import DictPack
reference = DictPack(str(base))
checks = 0
for direction, words in [("en-zh", ["apple","bank","contract","hello","ice cream","mother-in-law","don't","Aux","zzzznotaword"]),("zh-en",["安全","银行","你好","苹果","法律","學習","龘"])]:
    for word in words:
        expected = reference.lookup(direction,word,100)
        for page in range(max(1,len(expected))):
            out=Entry();total=c.c_uint()
            status=lib.dict_lookup(str(base).encode(),direction=="zh-en",word.encode(),page,c.byref(out),c.byref(total))
            assert status==(1 if expected else 0),(word,status)
            assert total.value==len(expected),(word,total.value,len(expected))
            if expected:
                actual=(out.head.decode(),out.phonetic.decode(),out.definition.decode())
                # Compare as a set: C reader preserves index order; legacy reader demotes proper names.
                assert actual in expected,(word,actual)
                assert len(actual[2])>0
            checks+=1
for pinyin in ["nihao","anquan","yinhang","zhong","lv","zzzzzz"]:
    cursor=c.c_uint32();more=c.c_bool();seen=[]
    for page in range(3):
        out=(Candidate*5)();count=lib.dict_candidates(str(base).encode(),pinyin.encode(),c.byref(cursor),out,c.byref(more))
        assert 0<=count<=5
        seen.extend(out[i].head.decode() for i in range(count))
        if not more.value:break
    assert all(seen)
    checks+=1
for word,want in [("aux","_aux.wav"),("COM1","_com1.wav"),("don't","don_t.wav"),("ice cream","ice_cream.wav")]:
    output=c.create_string_buffer(140);assert lib.dict_audio_name(word.encode(),output,140) and output.value.decode()==want
    checks+=1
handle=c.c_void_p();length=c.c_uint32()
assert lib.dict_wav_open(str(base.parent / "audio/apple.wav").encode(),c.byref(handle),c.byref(length))==0
assert length.value>0
libc=c.CDLL(None);libc.fclose.argtypes=[c.c_void_p];libc.fclose(handle);checks+=1
with tempfile.TemporaryDirectory() as temp:
    folder=Path(temp)
    for data in [b"short", b"apple"+b"\0"*99+struct.pack("<IH",99999999,10)]:
        (folder/"en-zh.idx").write_bytes(data);(folder/"en-zh.dat").write_bytes(b"x")
        out=Entry();total=c.c_uint();assert lib.dict_lookup(temp.encode(),False,b"apple",0,c.byref(out),c.byref(total))==-1
        checks+=1
    lib.dict_book_save.argtypes=[c.c_char_p,c.c_bool,c.c_char_p]
    lib.dict_book_save.restype=c.c_bool
    lib.dict_book_read.argtypes=[c.c_char_p,c.c_bool,c.POINTER(c.c_char*192)]
    books=folder/'nested/user'
    for i in range(23):
        assert lib.dict_book_save(str(books).encode(),True,f'word{i}'.encode())
    assert len((books/'favorites.txt').read_text().splitlines())==23
    assert lib.dict_book_save(str(books).encode(),True,b'word0')
    lines=(books/'favorites.txt').read_text().splitlines()
    assert len(lines)==23 and lines[-1]=='word0'
    for i in range(23):assert lib.dict_book_save(str(books).encode(),False,f'word{i}'.encode())
    assert len((books/'history.txt').read_text().splitlines())==20
    assert not lib.dict_book_save(str(books).encode(),True,b'bad\nentry')
    checks+=5
lib.dict_stereo_frames.argtypes=[c.POINTER(c.c_int16),c.c_size_t,c.POINTER(c.c_int16)]
lib.dict_stereo_frames.restype=None
mono=(c.c_int16*4)(32767,-32768,0,1234);stereo=(c.c_int16*8)()
lib.dict_stereo_frames(mono,4,stereo)
assert list(stereo)==[10922,10922,-10922,-10922,0,0,411,411]
assert len(stereo)//2==len(mono)  # stereo frames retain the original sample count/duration
checks+=2
lib.dict_scroll_target.argtypes=[c.c_int,c.c_int,c.c_int,c.c_int,c.c_bool]
for args,want in [((48,300,96,-1,False),24),((48,300,96,1,False),72),((0,300,96,-1,False),0),((288,300,96,1,False),300),((0,300,96,1,True),72),((72,300,96,-1,True),0)]:
    assert lib.dict_scroll_target(*args)==want
    checks+=1
lib.dict_clean_query.argtypes=[c.c_char_p]
for original,want in [("  apple \t","apple"),(" ice cream ","ice cream"),(" \n", ""),("  银行  ","银行")]:
    value=c.create_string_buffer(original.encode(),192);lib.dict_clean_query(value);assert value.value.decode()==want
    checks+=1
cursor=c.c_uint32();more=c.c_bool();pages=[]
for _ in range(3):
    out=(Candidate*5)();count=lib.dict_candidates(str(base).encode(),b'shi',c.byref(cursor),out,c.byref(more))
    assert count==5 and more.value
    pages.append([out[i].head.decode() for i in range(count)])
assert pages[0]!=pages[1] and pages[1]!=pages[2]
checks+=2
with tempfile.TemporaryDirectory() as temp:
    wav=Path(temp)/'broken.wav';wav.write_bytes(b'not a wave')
    assert lib.dict_wav_open(str(wav).encode(),c.byref(handle),c.byref(length))==-2
    assert lib.dict_wav_open(str(wav.parent/'missing.wav').encode(),c.byref(handle),c.byref(length))==-1
    fmt=struct.pack('<HHIIHH',3,1,48000,96000,2,16)
    payload=b'fmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',2)+b'00'
    wav.write_bytes(b'RIFF'+struct.pack('<I',len(payload)+4)+b'WAVE'+payload)
    assert lib.dict_wav_open(str(wav).encode(),c.byref(handle),c.byref(length))==-3
    checks+=3
audio_files = sorted((base.parent / 'audio').glob('*.wav'))
with tempfile.TemporaryDirectory() as temp:
    folder = Path(temp)
    definition = ('中文释义' * 1000).encode()
    phonetic = b'/long-phonetic/'
    payload = b'longword\0' + phonetic + b'\0' + definition
    (folder/'en-zh.dat').write_bytes(payload)
    (folder/'en-zh.idx').write_bytes(b'longword'+b'\0'*96+struct.pack('<IH',0,len(payload)))
    out=Entry();total=c.c_uint()
    assert lib.dict_lookup(temp.encode(),False,b'longword',0,c.byref(out),c.byref(total))==1
    assert out.head==b'longword' and out.phonetic==phonetic and out.definition==definition
    checks+=1
assert len(audio_files) == 30000
if '--skip-audio-sweep' in sys.argv[2:]:
    audio_files = []
for i, path in enumerate(audio_files, 1):
    output = c.create_string_buffer(140)
    assert lib.dict_audio_name(path.stem.upper().encode(), output, 140)
    assert output.value.decode() == path.name, path.name
    handle = c.c_void_p(); length = c.c_uint32()
    assert lib.dict_wav_open(str(path).encode(), c.byref(handle), c.byref(length)) == 0, path.name
    assert length.value > 0, path.name
    libc.fclose(handle)
    checks += 2
    if i % 100 == 0:
        print('WAV name/header check', i, '/', len(audio_files), flush=True)
print(json.dumps({"checks":checks,"status":"passed","wav_files":len(audio_files),"data":str(base)}))
