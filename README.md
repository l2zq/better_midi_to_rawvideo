**better_midi_to_rawvideo**

[toc]

## 编译:

> gcc \*.c -Ofast

_用了 mmap, 所以 Windows 应该不能用，对 mmap-load.c 稍作修改就能用了。_  
_也许不需要-Ofast，我不开也挺快的（（（_

## 使用:

_目前修改参数只能通过编辑源码_  
`param.`

- `filename`: MIDI 文件名
- `frame_w`, `frame_h`: 画面的宽、高
- `fps_up`, `fps_dn`: fps 由`fps_up/fps_dn`计算得到
- `draw_barborder`: 音符条边框开关
- `draw_keyboard`: 钢琴键盘开关
  - `keyboard_h`: 键盘高度
  - `blackkey_h`: 黑键高度
- `screen_height`: 屏幕高度代表的 MIDI tick 数  
  表达式`param.frame_h - (param.draw_keyboard ? param.keyboard_h : 0)`使一像素恰好为一 MIDI tick.

_(别改太离谱的参数，根本没有越界检查的)_

一些其他平常不需要改的参数：

- `tckk_poolsize`: 预先分配内存的音符数量（MIDI 中同时按下的键的个数。）
- `bars_poolsize`: 预先分配的音符条数量（屏幕上的音符条，指每一段有边界的矩形，即无音符的部分也是音符条）
- `queu_poolsize`: 预先分配的某队列的元素数量，目前这个队列用来存放设置节拍事件(Meta Set-Tempo)和有事件的 Tick 处的 Note-ON 事件数(0x90)。
  _(其实是两个队列，但省略细节)_

以上数值若足够大（并不用多大），运行时就不需要 malloc 分配内存。

文字字体：去看`text.c`，写得很烂，但很好改（  
字体是用的 [https://github.com/idispatch/raster-fonts](https://github.com/idispatch/raster-fonts)

## 视频输出：

程序向 stdout 输出 BGRA 视频数据，可以用 mpv 或 ffplay(ffmpeg)接收：  
mpv:

> ./a.out | mpv --demuxer=rawvideo --demuxer-rawvideo-{w=1920,h=1080,fps=60,size=8294400,format=BGRA}  
> \# 8294400 = 1920 _ 1080 _ 4  
> \# 可以用 --audio-file= 指定同时播放的音频文件

ffplay(ffmpeg):

> ./a.out | ffplay -f rawvideo -pixel_format bgra -video_size 1920x1080 -framerate 60 -i -
