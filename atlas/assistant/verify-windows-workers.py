"""Smoke-test the packaged Windows executables through their named IPC protocol."""
import ctypes as c
import mmap
from pathlib import Path
import subprocess
import sys
import uuid
import wave


class Message(c.LittleEndianStructure):
    _fields_ = [('role', c.c_uint32), ('length', c.c_uint32), ('text', c.c_char * 4097)]


class Chat(c.LittleEndianStructure):
    _fields_ = [('version', c.c_uint32), ('count', c.c_uint32), ('messages', Message * 6),
                ('result', c.c_uint32), ('reply_bytes', c.c_uint32), ('reply', c.c_char * 4096)]


class Voice(c.LittleEndianStructure):
    _fields_ = [('version', c.c_uint32), ('samples', c.c_uint32), ('text_bytes', c.c_uint32),
                ('pcm', c.c_int16 * 240000), ('text', c.c_char * 8192)]


def run(exe, model, request):
    name = 'Local\\AtlasAssistantSmoke-' + str(uuid.uuid4())
    with mmap.mmap(-1, c.sizeof(request), tagname=name) as exchange:
        exchange.write(bytes(request))
        subprocess.run([str(exe.resolve()), name, str(model.resolve())], check=True, timeout=240)
        return type(request).from_buffer_copy(exchange)


def main():
    install, gguf, whisper, sample = map(Path, sys.argv[1:])
    chat = Chat(version=1, count=1)
    prompt = b'What is 2 + 2? Reply with just the number.'
    chat.messages[0] = Message(role=1, length=len(prompt), text=prompt)
    chat = run(install / 'atlas-assistant-worker.exe', gguf, chat)
    reply = chat.reply[:chat.reply_bytes].decode('utf-8').strip()
    if chat.version != 1 or chat.result != 1 or not 0 < chat.reply_bytes <= 4096 or reply.rstrip('.') != '4':
        raise RuntimeError(f'Unexpected chat status/reply: {chat.result} {reply!r}')
    print('Chat worker returned:', reply)

    with wave.open(str(sample), 'rb') as audio:
        count = audio.getnframes()
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, 16000) or not 4000 <= count <= 240000:
            raise RuntimeError('Incorrect test fixture')
        pcm = audio.readframes(count)
    voice = Voice(version=1, samples=count)
    c.memmove(c.addressof(voice) + Voice.pcm.offset, pcm, len(pcm))
    voice = run(install / 'atlas-voice-worker.exe', whisper, voice)
    words = voice.text[:voice.text_bytes].decode('utf-8')
    if voice.version != 1 or not 0 < voice.text_bytes < 8192 or 'what your country can do' not in words.lower():
        raise RuntimeError(f'Unexpected transcript: {words!r}')
    print('Voice worker transcribed:', words)


if __name__ == '__main__':
    main()
