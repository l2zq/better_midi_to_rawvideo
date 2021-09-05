#define _GNU_SOURCE

#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <errno.h>

#include "util.h"
#include "file-load.h"
#include "midi.h"
#include "tckk.h"
#include "bars.h"
#include "queu.h"

#ifdef ENABLE_TEXT
#include "text.h"
#endif

#if defined(ENABLE_TEXT) && defined(ENABLE_METALIST)
#define _METALIST_ENABLE
#endif

#ifdef _METALIST_ENABLE
#define METALIST_LESS 8
#define METALIST_MID 16
#define METALIST_MAX 32
#define METALIST_EXTREME 256
#endif

#ifdef USE_MPV
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#define BGRA(R, G, B) (0xFF000000 + ((R) << 16) + ((G) << 8) + (B))
static const ui32 trkcolors[] = {                                           //
    BGRA(0xFF, 0x00, 0x00), BGRA(0x00, 0xFF, 0x00), BGRA(0x00, 0x00, 0xFF), //
    BGRA(0xFF, 0xFF, 0x00), BGRA(0x00, 0xFF, 0xFF), BGRA(0xFF, 0x00, 0xFF), //
    BGRA(0x7F, 0x00, 0x00), BGRA(0x00, 0x7F, 0x00), BGRA(0x00, 0x00, 0x7F), //
    BGRA(0x7F, 0x7F, 0x00), BGRA(0x00, 0x7F, 0x7F), BGRA(0x7F, 0x00, 0x7F), //
    BGRA(0x7F, 0xFF, 0x00), BGRA(0x00, 0x7F, 0xFF), BGRA(0x7F, 0x00, 0xFF), //
    BGRA(0xFF, 0x7F, 0x00), BGRA(0x00, 0xFF, 0x7F), BGRA(0xFF, 0x00, 0x7F)};
static const ui32 trkclrcnt = sizeof(trkcolors) / sizeof(ui32);
static const ui32 color_black = BGRA(0, 0, 0), color_white = BGRA(255, 255, 255);
static const ui32 color_bg = color_white, color_border = color_black;
static const bool g_iswhitekey[] = {1, 0, 1, 0, 1, /**/ 1, 0, 1, 0, 1, 0, 1};
static const bool g_white_left[] = {0, 0, 1, 0, 1, /**/ 0, 0, 1, 0, 1, 0, 1};
static const bool g_white_righ[] = {1, 0, 1, 0, 0, /**/ 1, 0, 1, 0, 1, 0, 0};
struct {
  const char *midifile;
  const char *audiofile;

  tk_t screen_height;
  bool draw_keyboard;
  ui16 keyboard_h, blackkey_h;
  ui16 frame_w, frame_h, fps_up, fps_dn;
  bool draw_barborder;

  size_t tckk_poolsize;
  size_t bars_poolsize;
  size_t queu_poolsize;
} param;
struct {
  ui16 key_x[129];
  ui16 *bar_y;
  ui16 fall_h;
  ui16 half_key;
  ui16 keyboard_y;
} consts;
struct {
  ui32 *data;
  size_t len;
} frame;
void real_main();

FILE *fp_out;

#ifdef USE_MPV
pid_t mpv_pid;
bool mpv_start() {
  int tmp_errno, pipe_r, pipe_w;
  {
    int pipefd[2];
    if (pipe(pipefd) == -1)
      return false;
    pipe_r = pipefd[0];
    pipe_w = pipefd[1];
    // fprintf(stderr, "a. ioctl(fd, F_GETPIPE_SZ) = %d\n", fcntl(pipefd[0], F_GETPIPE_SZ));
    // fprintf(stderr, "b. ioctl(fd, F_SETPIPE_SZ) = %d\n", fcntl(pipefd[0], F_SETPIPE_SZ, 1048576));
    // fprintf(stderr, "c. ioctl(fd, F_GETPIPE_SZ) = %d\n", fcntl(pipefd[0], F_GETPIPE_SZ));
  }

  switch ((mpv_pid = fork())) {
  case -1: // failed
    tmp_errno = errno;
    close(pipe_r);
    close(pipe_w);
    errno = tmp_errno;
    return false;
  case 0: // child process - close write
    close(pipe_w);
    char s1[32], s2[32], s3[32], s4[32], s5[32], s6[512], s7[512], s8[64]; //, s9[64];
    snprintf(s1, sizeof(s1), "--demuxer-rawvideo-w=%d", param.frame_w);
    snprintf(s2, sizeof(s2), "--demuxer-rawvideo-h=%d", param.frame_h);
    snprintf(s3, sizeof(s3), "--demuxer-rawvideo-fps=%d", param.fps_up / param.fps_dn);
    snprintf(s4, sizeof(s4), "--demuxer-rawvideo-size=%d", (ui32)param.frame_w * param.frame_h * 4);
    snprintf(s5, sizeof(s5), "fdclose://%d", pipe_r);
    snprintf(s6, sizeof(s6), "--force-media-title=midiRawVideo %s", param.midifile);
    if (param.audiofile)
      snprintf(s7, sizeof(s7), "--audio-file=%s", param.audiofile);
    else
      s7[0] = '\0';
    snprintf(s8, sizeof(s8), "--demuxer-max-bytes=%ld", frame.len * 1 * param.fps_up / param.fps_dn);
    // snprintf(s9, sizeof(s9), "--demuxer-max-back-bytes=%ld", frame.len * 1 * param.fps_up / param.fps_dn);
    fprintf(stderr, "s8 = %s\n", s8);
    if (execlp("mpv", "mpv",
               "--demuxer=rawvideo",
               "--demuxer-rawvideo-format=BGRA",
               s1, s2, s3, s4, s5, s6, s7, s8, NULL) == -1) {
      tmp_errno = errno;
      close(pipe_r);
      errno = tmp_errno;
    }
    return false;
  default: // parent process - close read, fdopen write
    close(pipe_r);
    if ((fp_out = fdopen(pipe_w, "wb")) == NULL) {
      tmp_errno = errno;
      close(pipe_r);
      errno = tmp_errno;
      return false;
    }
    return true;
  }
}
void mpv_wait() {
  waitpid(mpv_pid, NULL, 0);
}
#endif

void set_screen_height(void) {
  param.screen_height = param.frame_h - (param.draw_keyboard ? param.keyboard_h : 0);
  // param.screen_height <<= 1;
  // param.screen_height = midi.divs / 2;
}
int main(int argc, char **argv) {
  fp_out = stdout;

  param.midifile = NULL;
  param.audiofile = NULL;

  signal(SIGPIPE, SIG_IGN);

  if (argc > 1) {
    param.midifile = argv[1];
    if (argc > 2) {
      param.audiofile = argv[2];
    }
  }

  param.frame_w = 1280;
  param.frame_h = 720;
  param.fps_up = 144; // fps = fps_up / fps_dn
  param.fps_dn = 1;

  param.draw_barborder = true;

  param.draw_keyboard = true;
  param.keyboard_h = param.frame_h / 10;
  param.blackkey_h = param.frame_h / 15;

  {
    frame.len = param.frame_w * param.frame_h * sizeof(ui32);
    size_t alloc_len = frame.len + 8 * param.frame_w * sizeof(ui32);
    if ((frame.data = malloc(alloc_len)) == NULL) {
      perror("malloc frame");
      return -1;
    }
    // for (ui32 i = 0; i < param.frame_w * param.frame_h; i++)
    //   frame.data[i] = BGRA(0xff, 0x00, 0xff);
    // no need to clear frame
  }

#ifdef ENABLE_TEXT
  if (text_init(frame.data, param.frame_w, param.frame_h) == -1) {
    perror("text_init");
    free(consts.bar_y);
    free(frame.data);
    return -1;
  }
#endif

  int ret = 0, midi_ret;
  if (file_load(param.midifile)) {
    if ((midi_ret = midi_init(file.mem, file.len)) == MIDI_OK) {
      set_screen_height();
      if ((consts.bar_y = malloc(sizeof(ui16) * (param.screen_height + 1))) != NULL) {
        { // init consts
          consts.keyboard_y = param.frame_h - (param.draw_keyboard ? param.keyboard_h : 0);
          consts.half_key = param.frame_w / 128 / 2;
          for (ui16 x = 0; x <= 128; x++)
            consts.key_x[x] = x * param.frame_w / 128;
          consts.fall_h = param.frame_h;
          if (param.draw_keyboard) {
            consts.fall_h -= param.keyboard_h;
          }
          for (tk_t u = 0; u <= param.screen_height; u++)
            consts.bar_y[u] = consts.fall_h - u * consts.fall_h / param.screen_height;
        }
        param.tckk_poolsize = 16384;
        if (tckk_init(midi.ntrk, param.tckk_poolsize) == 0) {
          param.bars_poolsize = param.screen_height * 128;
          if (bars_init(param.bars_poolsize) == 0) {
            param.queu_poolsize = 2048;
            if (queu_init(param.queu_poolsize) == 0) {
              fprintf(stderr, "\e[2Kmidi: type %hu ntrk %hu divs %hu\n", midi.type, midi.ntrk, midi.divs);
#ifdef USE_MPV
              if (!mpv_start())
                ret = -1, perror("mpv_start");
              else
#endif
                real_main();
              queu_free();
            } else
              ret = -1, perror("queu_init");
            bars_free();
          } else
            ret = -1, perror("bars_init");
          tckk_free();
        } else
          ret = -1, perror("tckk_init");
        free(consts.bar_y);
      } else
        ret = -1, perror("init consts - malloc bar y");
      midi_free();
    } else if (midi_ret == MIDI_E_SEE_ERRNO)
      ret = -1, perror("midi_init");
    else
      ret = 1, fprintf(stderr, "midi_init: error code %d\n", midi_ret);
    file_free();
  } else
    ret = -1, perror("file_load");

#ifdef ENABLE_TEXT
  text_free();
#endif

  free(frame.data);
  if (fp_out != stdout)
    fclose(fp_out);

#ifdef USE_MPV
  mpv_wait();
#endif

  return ret;
}

#define LIST_META 0xff

#define LIST_FEND 0x2f
#define LIST_MPQN 0x51
#define LIST_NOTE_UP 8
#define LIST_NOTE_DN 9

#ifdef _METALIST_ENABLE
TNumList q_meta;
#endif

TNumList q_list;
bool tick_ended;
tk_t tick_procd; // processed tick count
void tick_begin() {
  midi.trk = midi.trks;
  for (int it = 0; it < midi.ntrk; it++, midi.trk++)
    if (midi.trk->track_left)
      midi.trk->last_delta = mtrk_dt();
  tick_procd = 0;
  tick_ended = false;
}
tk_t tick_runto(tk_t wanted) { // should call everytime scr top time increases (every tick)
  if (tick_ended)
    return tick_procd;
  if (tick_procd < wanted) {
    bool end_old_no[128] = {false};
    no_t oldnoteids[128];
    for (int jk = 0; jk < 128; jk++)
      oldnoteids[jk] = tckk_keys[jk].tail->note_id;
    do {
      bool lef;
      ui32 mdt = MIDI_MAXDT + 1, dt,
           notedn_cnt = 0,
           noteup_cnt = 0;
      midi.trk = midi.trks;
      for (int it = 0; it < midi.ntrk; it++, midi.trk++)
        if (midi.trk->track_left) {
          if ((dt = midi.trk->last_delta) == 0) {
            while (true) {
              lef = mtrk_evt();
              // process event
              switch (midi.evt.b >> 4) {
              case 0x9:
                if (midi.evt.a2 > 0) {
                  tckk_keydn(it, midi.evt.b & 0xf, midi.evt.a1);
                  notedn_cnt++;
                  break;
                }
              case 0x8: {
                no_t up_id = tckk_keyup(it, midi.evt.b & 0xf, midi.evt.a1);
                if (up_id > 0) {
                  noteup_cnt++;
                  if (up_id == oldnoteids[midi.evt.a1])
                    end_old_no[midi.evt.a1] = true;
                }
                break;
              }
              case 0xf:
                if (midi.evt.b == 0xff) {
#ifdef _METALIST_ENABLE
                  TNum *tnm = TNL_push(&q_meta);
                  tnm->tick = tick_procd;
                  tnm->type = LIST_META;
                  tnm->numb_byte = midi.evt.msys.type;
                  tnm->numb_ui16 = (ui16)it;
                  tnm->numb_ui32 = midi.evt.msys.size;
                  tnm->numb_ui64 = (ui64)midi.evt.msys.data;
#endif
                  if (midi.evt.msys.type == 0x51 && midi.evt.msys.size >= 3) {
                    ui32 mpqn = midi.evt.msys.data[0];
                    mpqn = (mpqn << 8) | midi.evt.msys.data[1];
                    mpqn = (mpqn << 8) | midi.evt.msys.data[2];
                    if (mpqn) {
                      TNum *tnm = TNL_push(&q_list);
                      tnm->tick = tick_procd;
                      tnm->type = LIST_MPQN;
                      tnm->numb_ui32 = mpqn;
                    }
                  }
                }
              }
              if (lef) {
                if ((dt = mtrk_dt()) > 0) {
                  if ((midi.trk->last_delta = dt) < mdt)
                    mdt = dt;
                  break;
                }
              } else {
                midi.trk->track_left = false;
                break;
              }
            }
          } else if (dt < mdt)
            mdt = dt;
        }

      for (int jk = 0; jk < 128; jk++) {
        TCKNote *tail = tckk_keys[jk].tail;
        no_t old_noteid = oldnoteids[jk];
        no_t new_noteid = tail->note_id;
        if (new_noteid != old_noteid) {
          Bar *bar = bars_add_bar(jk, tick_procd, end_old_no[jk], new_noteid > old_noteid);
          bar->n_id = new_noteid;
          bar->trak = tail->trak;
          bar->chan = tail->chan;
          oldnoteids[jk] = new_noteid;
        }
        end_old_no[jk] = false;
      }

      TNum *tnm;
      tnm = TNL_push(&q_list);
      tnm->tick = tick_procd;
      tnm->type = LIST_NOTE_DN;
      tnm->numb_ui32 = notedn_cnt;

      tnm = TNL_push(&q_list);
      tnm->tick = tick_procd;
      tnm->type = LIST_NOTE_UP;
      tnm->numb_ui32 = noteup_cnt;

      if (mdt > MIDI_MAXDT) {
        tnm = TNL_push(&q_list);
        tnm->tick = tick_procd;
        tnm->type = LIST_FEND;
        tick_ended = true;
      } else {
        tick_procd += mdt;
        for (int it = 0; it < midi.ntrk; it++)
          midi.trks[it].last_delta -= mdt;
      }
    } while (tick_procd < wanted && !tick_ended);
  }
  return tick_procd;
}

static inline ui32 calc_border_color(ui32 bdrcolor) {
  byte *clr = (byte *)&bdrcolor;
  for (int i = 0; i < 3; i++)
    clr[i] *= 0.75;
  return bdrcolor;
}

static inline void draw_keyboard_borders() {
  for (ui32 x = 0; x < param.frame_w; x++)
    frame.data[consts.fall_h * param.frame_w + x] = color_border;
  ui16 bar_u = consts.fall_h + 1, bar_l, bar_r,
       bar_y = consts.fall_h + param.blackkey_h,
       bar_z = bar_y - 1;
  for (int jk = 0; jk < 128; jk++) {
    ui16 chord_k = jk % 12;
    bar_r = consts.key_x[jk + 1] - 1;
    if (g_iswhitekey[chord_k]) { // white key border
      if (chord_k == 4 || chord_k == 11)
        for (ui32 y = bar_u; y < param.frame_h; y++)
          frame.data[y * param.frame_w + bar_r] = color_black;
    } else { // black key border
      bar_l = consts.key_x[jk];
      for (ui32 y = bar_u; y < bar_y; y++) {
        frame.data[y * param.frame_w + bar_l] = color_black;
        frame.data[y * param.frame_w + bar_r] = color_black;
      }
      for (ui32 x = bar_l; x < bar_r; x++)
        frame.data[bar_z * param.frame_w + x] = color_black;
    }
  }
}

void real_main() {
  TNL_ini(&q_list);
#ifdef _METALIST_ENABLE
  TNL_ini(&q_meta);
#endif
  // setup bars
  bars.screen_bot = 0;
  bars.screen_top = bars.screen_bot + param.screen_height;
  for (int jk = 0; jk < 128; jk++) {
    Bar *bar = bars_add_bar(jk, 0, false, false);
    bar->n_id = 0;
  }
  // begin tick
  tick_begin();
  bool midi_endd = false;
  ui32 mpqn = 500000;
  tm_t curr_time = 0,
       next_midi = 0, next_frame = 0;
  tm_t midi_interval = mpqn * param.fps_up,
       frame_interval = 1000000 * midi.divs * param.fps_dn;

  ui64 frame_cnt = 0, notedn_cnt = 0, noteup_cnt = 0, polyphony_peak = 0;
#ifdef ENABLE_TEXT
  char message[256];
#endif

#ifdef _METALIST_ENABLE
  ui32 d1_second_frame_count = param.fps_up / param.fps_dn,
       d2_second_frame_count = param.fps_up / param.fps_dn / 2,
       d4_second_frame_count = param.fps_up / param.fps_dn / 4,
       d8_second_frame_count = param.fps_up / param.fps_dn / 8;
#endif

  while (true) {
    bool run_midi = (curr_time == next_midi);
    if (run_midi) {
      tick_runto(bars.screen_top);
      // if (bars.screen_bot >
      //   midi_endd = true;    -> change to FEND
      TNum *tnm;
      tnm = q_list.head.next;
      while (tnm && tnm->tick <= bars.screen_bot) {
        switch (tnm->type) {
        case LIST_NOTE_UP:
          noteup_cnt += tnm->numb_ui32;
          break;
        case LIST_NOTE_DN:
          notedn_cnt += tnm->numb_ui32;
          {
            ui64 polyphony = notedn_cnt - noteup_cnt;
            if (polyphony > polyphony_peak)
              polyphony_peak = polyphony;
          }
          break;
        case LIST_FEND:
          midi_endd = true;
          break;
        case LIST_MPQN:
          mpqn = tnm->numb_ui32, midi_interval = mpqn * param.fps_up;
          break;
        }
        tnm = TNL_pop(&q_list);
      }
      next_midi += midi_interval;
    }
    if (curr_time == next_frame) {
      frame_cnt++;
      next_frame += frame_interval;
      KBarList *kbls = bars.keys;

      for (int jk = 0; jk < 128; jk++, kbls++) {
        Bar *bar, *nex = bars_del_bef(kbls); // iterate
        ui32 bar_color, bdr_color;
        tk_t bar_beg = bars.screen_bot, bar_end, bar_up, bar_dn; // bar ticks
        ui16 bar_u, bar_d,                                       //
            bar_l = consts.key_x[jk],                            // bar l, r, u, d
            bar_r = consts.key_x[jk + 1];                        //
        bool bdr_bot, bdr_top, bdr_lr, bar_long_enough;
        while ((bar = nex)) {
          nex = bar->next, bar_end = bar->bend;

          if ((bar_dn = bar_beg - bars.screen_bot) < 0) // calc bar_dn tick
            bar_dn = 0;                                 //
          if (bar_end > bars.screen_top)                //
            bar_up = param.screen_height;               //
          else                                          //
            bar_up = bar_end - bars.screen_bot;         // calc bar_up tick

          bar_d = consts.bar_y[bar_dn]; // bar down y
          bar_u = consts.bar_y[bar_up]; // bar   up y

          bdr_top = false, bdr_bot = false, bdr_lr = param.draw_barborder;
          bar_long_enough = true;

          if (bar_d - bar_u < 3)
            bdr_lr = false, bar_long_enough = false;

          if (bar->n_id == 0)                             // bar color
            bar_color = color_bg;                         // no key -> bg color
          else {                                          //
            bar_color = trkcolors[bar->trak % trkclrcnt]; //
            bdr_color = calc_border_color(bar_color);
            if (!bar_long_enough)
              bar_color = bdr_color;
            if (bdr_lr) {
              bar_r--;
              for (ui32 y = bar_u; y < bar_d; y++) {
                frame.data[y * param.frame_w + bar_l] = bdr_color;
                frame.data[y * param.frame_w + bar_r] = bdr_color;
              }
              bar_l++;
            }
          }
          if (bar->n_id > 0 && bar_long_enough) {
            bdr_bot = bar->nbeg == bar_beg;   // whether draw bottom border
            if (bar_end == TICK_INF)          //
              bdr_top = false;                //
            else                              //
              bdr_top = bar_end == bar->nend; // whether draw top border

            if (param.draw_barborder) {
              if (bdr_bot) {
                bar_d--;
                for (ui32 x = bar_l; x < bar_r; x++)
                  frame.data[bar_d * param.frame_w + x] = bdr_color;
              }
              if (bdr_top) {
                for (ui32 x = bar_l; x < bar_r; x++)
                  frame.data[bar_u * param.frame_w + x] = bdr_color;
                bar_u++;
              }
            }
          }
          for (ui32 y = bar_u; y < bar_d; y++) {
            for (ui32 x = bar_l; x < bar_r; x++)
              frame.data[y * param.frame_w + x] = bar_color;
          }
          if (bdr_lr && bar->n_id > 0) {
            bar_r++;
            bar_l--;
          }
          bar_beg = bar_end;
        }
        // draw keyboard
        if (param.draw_keyboard) {
          ui16 chord_k = jk % 12;
          bool iswhite = g_iswhitekey[chord_k];
          bar_u = consts.fall_h + 1;
          bar_d = consts.fall_h + param.blackkey_h;
          if (iswhite) {
            bar_color = color_white;
            if (chord_k == 4 || chord_k == 11)
              bar_r--;
          } else {
            bar_l++;
            bar_r--;
            bar_d--;
            bar_color = color_black;
          }
          if ((bar = kbls->head.next)->n_id > 0) // no NULL check, bec'of design
            bar_color = trkcolors[bar->trak % trkclrcnt];
          for (ui32 y = bar_u; y < bar_d; y++)
            for (ui32 x = bar_l; x < bar_r; x++)
              frame.data[y * param.frame_w + x] = bar_color;
          if (iswhite) {
            bar_u = bar_d;
            bar_d = param.frame_h;
            if (g_white_left[chord_k])
              if ((bar_l -= consts.half_key) < 0)
                bar_l = 0;
            if (g_white_righ[chord_k])
              if ((bar_r += consts.half_key) > param.frame_w)
                bar_r = param.frame_w;
            for (ui32 y = bar_u; y < bar_d; y++) {
              for (ui32 x = bar_l; x < bar_r; x++)
                frame.data[y * param.frame_w + x] = bar_color;
              if (bar_l > 0)
                frame.data[y * param.frame_w + bar_l - 1] = color_border;
            }
          }
        }
      }
      if (param.draw_keyboard)
        draw_keyboard_borders();
        // draw text
#ifdef ENABLE_TEXT
      {
        ui32 secs = frame_cnt * param.fps_dn / param.fps_up, mins;
        ui32 frms = frame_cnt - secs * param.fps_up / param.fps_dn;
        mins = secs / 60, secs = secs % 60;

        if (mins == 0 && secs < 3) {
          if (param.audiofile)
            snprintf(message, sizeof(message),
                     "文件: %s\n"
                     "音频: %s\n"
                     "类型: %hu 轨道数: %hu 分辨率: %hu",
                     param.midifile, param.audiofile, midi.type, midi.ntrk, midi.divs);
          else
            snprintf(message, sizeof(message),
                     "文件: %s\n"
                     "类型: %hu 轨道数: %hu 分辨率: %hu",
                     param.midifile, midi.type, midi.ntrk, midi.divs);
          text_draw_utf8(message, 64, 64);
        }

#ifdef _METALIST_ENABLE
        {
          static ui64 last_event_increase = 0, last_event_count = 0;
          bool arrow_not_drawn = true;
          TNum *tnm, *nex = q_meta.head.next;
          if (nex) {
            if (q_meta.cnt > last_event_count)
              last_event_increase = frame_cnt;
            si16 dx = param.frame_w - 480, dy = 16;
            snprintf(message, sizeof(message), "Tick ⬇ Meta事件");
            text_draw_utf8(message, dx, dy);
            while ((tnm = nex) && (dy += TEXT_LINE_HEIGHT) < (consts.keyboard_y - TEXT_LINE_HEIGHT)) {
              nex = tnm->next;
              byte *data = (byte *)tnm->numb_ui64;
              switch (tnm->numb_byte) {
              case 0x01:
              case 0x02:
              case 0x03:
              case 0x04:
              case 0x05:
              case 0x06:
              case 0x07:
                int i = snprintf(message, sizeof(message), "%6ld:%02hd %hhd:", tnm->tick, tnm->numb_ui16, tnm->numb_byte);
                for (int j = 0; i < sizeof(message) - 2 && j < tnm->numb_ui32; i++, j++)
                  message[i] = data[j];
                message[i] = '\0';
                break;
              case 0x2f:
                snprintf(message, sizeof(message), "%6ld:轨道#%hd结束",
                         tnm->tick, tnm->numb_ui16);
                break;
              case 0x51: {
                ui32 x_mpqn = data[0];
                x_mpqn = (x_mpqn << 8) | data[1];
                x_mpqn = (x_mpqn << 8) | data[2];
                snprintf(message, sizeof(message), "%6ld:变速 BPM=%.3lf",
                         tnm->tick, 60000000.0 / x_mpqn);
                break;
              }
              default:
                snprintf(message, sizeof(message), "%6ld:#%02hd %02hhx L=%d",
                         tnm->tick, tnm->numb_ui16, tnm->numb_byte, tnm->numb_ui32);
              }
              text_draw_utf8(message, dx, dy);
              if (arrow_not_drawn && tnm->tick >= bars.screen_bot) {
                text_draw_utf8("⟶", dx - 16, dy);
                arrow_not_drawn = false;
              }
            }
            if (frame_cnt > d2_second_frame_count) {
              ui64 frame_since = frame_cnt - last_event_increase;
              ui32 secs_since = frame_since * param.fps_dn / param.fps_up;
              ui32 frms_since = frame_since - secs_since * param.fps_up / param.fps_dn;
              if (q_meta.cnt > METALIST_MID) {
                if (q_meta.cnt > METALIST_MAX) {
                  if (q_meta.cnt > METALIST_EXTREME) { // extreme ->
                    nex = q_meta.head.next;
                    while ((tnm = nex) && (q_meta.cnt > METALIST_MAX) && (tnm->tick < bars.screen_bot))
                      nex = TNL_pop(&q_meta);
                  } else if (q_meta.head.next->tick < bars.screen_bot) // max -> extreme
                    TNL_pop(&q_meta);
                } else if (frame_since >= d8_second_frame_count && frame_since % d8_second_frame_count == 0) // mid -> max
                  TNL_pop(&q_meta);
              } else if (q_meta.cnt > METALIST_LESS) { // less -> mid
                if (frame_since >= d4_second_frame_count && frame_since % d4_second_frame_count == 0)
                  TNL_pop(&q_meta);
              } else if (frame_since >= d1_second_frame_count && frms_since == 0) // -> less
                TNL_pop(&q_meta);
              // snprintf(message, sizeof(message), "frame_since = %lu\n", frame_since);
              // text_draw_utf8(message, 128, 128);
            }
          }
          last_event_count = q_meta.cnt;
        }
#endif
        snprintf(message, sizeof(message), "复音 %9lu\nBPM %10.3lf",
                 notedn_cnt - noteup_cnt,
                 60000000.0 / mpqn);
        text_draw_utf8(message, 0, 0);
        snprintf(message, sizeof(message), "音符: %9lu\n时间: %02d:%02d.%03df\nTick: %9ld (%ld)",
                 notedn_cnt,
                 mins, secs, frms,
                 bars.screen_bot, tick_procd);
        text_draw_utf8(message, 0, consts.keyboard_y - 3 * TEXT_LINE_HEIGHT);
      }
#endif
      if (fwrite(frame.data, frame.len, 1, fp_out) != 1)
        break;
      if (midi_endd)
        break;
    }
    if (run_midi) {
      bars.screen_bot++;
      bars.screen_top = bars.screen_bot + param.screen_height;
    }
    curr_time = next_midi < next_frame ? next_midi : next_frame;
  }
  fprintf(stderr, "\e[2Kframe_count = %lu, tick_count = %ld, note_dn = %lu, poly_peak = %lu\n",
          frame_cnt, tick_procd, notedn_cnt, polyphony_peak);

#ifdef _METALIST_ENABLE
  TNL_clr(&q_meta);
#endif
  TNL_clr(&q_list);
}
