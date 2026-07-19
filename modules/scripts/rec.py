import time
import audioplayer

def main(env, args):
    recorder = audioplayer.AudioRecorder(
        mclk=48, bck=47, ws=21, din=14,
        i2c_sda=18, i2c_scl=8,
        i2s_num=1,
        channels=1,
        mic_gain=8,
        i2c_shared=True,
    )

    recorder.record("/flash/note.wav", seconds=5)

    while recorder.is_recording():
        print(recorder.diagnostics())
        time.sleep_ms(200)

    bytes_written = recorder.stop()
    print("recorded", bytes_written, "bytes")
    recorder.deinit()
