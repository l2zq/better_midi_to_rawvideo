#include <stdio.h>
#include <strings.h>
#include "util.h"
#include "mmap-load.h"
#include "midi.h"
#include "tckk.h"
#include "bars.h"
#include "queu.h"
#include "text.h"

#define BGRA(R, G, B) (0xFF000000 + ((R) << 16) + ((G) << 8) + (B))
const ui32 trkcolors[] = {                                                  //
    BGRA(0xFF, 0x00, 0x00), BGRA(0x00, 0xFF, 0x00), BGRA(0x00, 0x00, 0xFF), //
    BGRA(0xFF, 0xFF, 0x00), BGRA(0x00, 0xFF, 0xFF), BGRA(0xFF, 0x00, 0xFF), //
    BGRA(0x7F, 0x00, 0x00), BGRA(0x00, 0x7F, 0x00), BGRA(0x00, 0x00, 0x7F), //
    BGRA(0x7F, 0x7F, 0x00), BGRA(0x00, 0x7F, 0x7F), BGRA(0x7F, 0x00, 0x7F), //
    BGRA(0x7F, 0xFF, 0x00), BGRA(0x00, 0x7F, 0xFF), BGRA(0x7F, 0x00, 0xFF), //
    BGRA(0xFF, 0x7F, 0x00), BGRA(0x00, 0xFF, 0x7F), BGRA(0xFF, 0x00, 0x7F)};
const ui32 trkclrcnt = sizeof(trkcolors) / sizeof(ui32);
const ui32 color_black = BGRA(0, 0, 0), color_white = BGRA(255, 255, 255);
const ui32 color_bg = color_white, color_border = color_black;
const bool g_iswhitekey[] = {1, 0, 1, 0, 1, /**/ 1, 0, 1, 0, 1, 0, 1};
const bool g_white_left[] = {0, 0, 1, 0, 1, /**/ 0, 0, 1, 0, 1, 0, 1};
const bool g_white_righ[] = {1, 0, 1, 0, 0, /**/ 1, 0, 1, 0, 1, 0, 0};
struct {
  const char *filename;

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
int main(void) {
  param.filename = NULL;

  param.frame_w = 1280;
  param.frame_h = 720;
  param.fps_up = 120; // fps = fps_up / fps_dn
  param.fps_dn = 1;

  param.draw_barborder = true;

  param.draw_keyboard = true;
  param.keyboard_h = param.frame_h / 10;
  param.blackkey_h = param.frame_h / 15;

  param.screen_height = param.frame_h - (param.draw_keyboard ? param.keyboard_h : 0);

  param.tckk_poolsize = 16384;
  param.bars_poolsize = param.screen_height * 128;
  param.queu_poolsize = 2048;

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
  {
    if ((consts.bar_y = malloc(sizeof(ui16) * (param.screen_height + 1))) == NULL) {
      perror("malloc bar y");
      free(frame.data);
      return -1;
    }
    consts.keyboard_y = param.frame_h - (param.draw_keyboard ? param.keyboard_h : 0);
    consts.half_key = param.frame_w / 128 / 2;
    for (ui16 x = 0; x <= 128; x++)
      consts.key_x[x] = x * param.frame_w / 128;
    consts.fall_h = param.frame_h;
    if (param.draw_keyboard) {
      consts.fall_h -= param.keyboard_h;
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
    for (tk_t u = 0; u <= param.screen_height; u++)
      consts.bar_y[u] = consts.fall_h - u * consts.fall_h / param.screen_height;
  }

  if (text_init(frame.data, param.frame_w, param.frame_h) == -1) {
    perror("text_init");
    free(consts.bar_y);
    free(frame.data);
    return -1;
  }

  int ret = 0, midi_ret;
  if (file_load(param.filename)) {
    if ((midi_ret = midi_init(file.mem, file.len)) == MIDI_OK) {
      if (tckk_init(midi.ntrk, param.tckk_poolsize) == 0) {
        if (bars_init(param.bars_poolsize) == 0) {
          if (queu_init(param.queu_poolsize) == 0) {
            fprintf(stderr, "midi: type %d ntrk %d divs %d\n", midi.type, midi.ntrk, midi.divs);
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
      midi_free();
    } else if (midi_ret == MIDI_E_SEE_ERRNO)
      ret = -1, perror("midi_init");
    else
      ret = 1, fprintf(stderr, "midi_init: error code %d\n", midi_ret);
    file_free();
  } else
    ret = -1, perror("file_load");

  text_free();

  free(consts.bar_y);
  free(frame.data);
  return ret;
}

TNumList q_mpqn;
TNumList q_note_dn;
TNumList q_note_up;
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
                if (midi.evt.b == 0xff && midi.evt.msys.type == 0x51 && midi.evt.msys.size >= 3) {
                  ui32 mpqn = midi.evt.msys.data[0];
                  mpqn = (mpqn << 8) | midi.evt.msys.data[1];
                  mpqn = (mpqn << 8) | midi.evt.msys.data[2];
                  if (mpqn) {
                    TNum *tnm = TNL_push(&q_mpqn);
                    tnm->tick = tick_procd;
                    tnm->numb = mpqn;
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
      tnm = TNL_push(&q_note_dn);
      tnm->tick = tick_procd;
      tnm->numb = notedn_cnt;

      tnm = TNL_push(&q_note_up);
      tnm->tick = tick_procd;
      tnm->numb = noteup_cnt;

      if (mdt > MIDI_MAXDT) {
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
void real_main() {
  TNL_ini(&q_mpqn);
  TNL_ini(&q_note_dn);
  TNL_ini(&q_note_up);
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
  ui32 frame_cnt = 0, notedn_cnt = 0, noteup_cnt = 0;

  char message[256];

  while (true) {
    bool run_midi = (curr_time == next_midi);
    if (run_midi) {
      if (bars.screen_bot > tick_runto(bars.screen_top))
        midi_endd = true;
      // fprintf(stderr, "tick_runto %d\n", bars.screen_top);
      TNum *tnm;
      tnm = q_mpqn.head.next;
      while (tnm && tnm->tick <= bars.screen_bot) {
        mpqn = tnm->numb, midi_interval = mpqn * param.fps_up;
        tnm = TNL_pop(&q_mpqn);
      }
      tnm = q_note_dn.head.next;
      while (tnm && tnm->tick <= bars.screen_bot) {
        notedn_cnt += tnm->numb;
        tnm = TNL_pop(&q_note_dn);
      }
      tnm = q_note_up.head.next;
      while (tnm && tnm->tick <= bars.screen_bot) {
        noteup_cnt += tnm->numb;
        tnm = TNL_pop(&q_note_up);
      }

      next_midi += midi_interval;
    }
    if (curr_time == next_frame) {
      frame_cnt++;
      next_frame += frame_interval;
      KBarList *kbls = bars.keys;
      for (int jk = 0; jk < 128; jk++, kbls++) {
        Bar *bar, *nex = bars_del_bef(kbls); // iterate
        ui32 bar_color;
        tk_t bar_beg = bars.screen_bot, bar_end, bar_up, bar_dn; // bar ticks
        ui16 bar_u, bar_d,                                       //
            bar_l = consts.key_x[jk],                            // bar l, r, u, d
            bar_r = consts.key_x[jk + 1];                        //
        bool bdr_bot, bdr_top, bdr_lr, tmp_lr;
        while ((bar = nex)) {
          // break;
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
          tmp_lr = true;

          if (bar_d - bar_u < 3)
            bdr_lr = false, tmp_lr = false;

          if (bar->n_id == 0)                             // bar color
            bar_color = color_bg;                         // no key -> bg color
          else {                                          //
            bar_color = trkcolors[bar->trak % trkclrcnt]; //
            if (bdr_lr) {
              bar_r--;
              for (ui32 y = bar_u; y < bar_d; y++) {
                frame.data[y * param.frame_w + bar_l] = color_border;
                frame.data[y * param.frame_w + bar_r] = color_border;
              }
              bar_l++;
            }
          }
          if (bar->n_id > 0 && tmp_lr) {
            bdr_bot = bar->nbeg == bar_beg;   // whether draw bottom border
            if (bar_end == TICK_INF)          //
              bdr_top = false;                //
            else                              //
              bdr_top = bar_end == bar->nend; // whether draw top border

            if (param.draw_barborder) {

              if (bdr_bot) {
                bar_d--;
                for (ui32 x = bar_l; x < bar_r; x++)
                  frame.data[bar_d * param.frame_w + x] = color_border;
              }
              if (bdr_top) {
                for (ui32 x = bar_l; x < bar_r; x++)
                  frame.data[bar_u * param.frame_w + x] = color_border;
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
      // draw text
      {
        snprintf(message, sizeof(message), "P %9d\nBPM %7.3f",
                 (si32)notedn_cnt - noteup_cnt,
                 60000000.0 / mpqn);
        text_drawTxt(0, 0, message);

        ui32 secs = frame_cnt * param.fps_dn / param.fps_up, mins;
        ui32 frms = frame_cnt - secs * param.fps_up / param.fps_dn;
        mins = secs / 60, secs = secs % 60;
        snprintf(message, sizeof(message), "Note: %9d\nTime: %02d:%02d+%03d Tick: %ld",
                 notedn_cnt,
                 mins, secs, frms,
                 bars.screen_bot);
        text_drawTxt(0, consts.keyboard_y - 2 * text_font_h, message);
      }

      if (fwrite(frame.data, frame.len, 1, stdout) != 1)
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
  fprintf(stderr, "frame_count = %d, tick_count = %ld, note_dn = %d\n", frame_cnt, tick_procd, notedn_cnt);
  TNL_clr(&q_mpqn);
  TNL_clr(&q_note_dn);
  TNL_clr(&q_note_up);
}
