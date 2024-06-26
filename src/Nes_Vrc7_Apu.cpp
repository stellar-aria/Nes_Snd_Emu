#include "Nes_Vrc7_Apu.h"

extern "C" {
#include "emu2413.h"
}

#include <cstring>

int const period = 36;  // NES CPU clocks per FM clock

Nes_Vrc7_Apu::Nes_Vrc7_Apu() {
}

std::error_condition Nes_Vrc7_Apu::init() {
  opll = OPLL_new(3579545, 3579545 / 72);
  if (opll == nullptr) {
    return std::make_error_condition(std::errc::not_enough_memory);
  }
  OPLL_setChipType((OPLL*)opll, OPLL_VRC7_TONE);

  set_output(nullptr);
  volume(1.0);
  reset();
  return {};
}

Nes_Vrc7_Apu::~Nes_Vrc7_Apu() {
  if (opll != nullptr) {
    OPLL_delete((OPLL*)opll);
  }
}

void Nes_Vrc7_Apu::set_output(Blip_Buffer* buf) {
  for (auto& osc : oscs) {
    osc.output = buf;
  }
  output_changed();
}

void Nes_Vrc7_Apu::output_changed() {
  mono.output = oscs[0].output;
  for (int i = osc_count; --i != 0;) {
    if (mono.output != oscs[i].output) {
      mono.output = nullptr;
      break;
    }
  }

  if (mono.output != nullptr) {
    for (int i = osc_count; --i != 0;) {
      mono.last_amp += oscs[i].last_amp;
      oscs[i].last_amp = 0;
    }
  }
}

void Nes_Vrc7_Apu::reset() {
  addr = 0;
  next_time = 0;
  mono.last_amp = 0;

  for (int i = osc_count; --i >= 0;) {
    Vrc7_Osc& osc = oscs[i];
    osc.last_amp = 0;
    for (unsigned char& reg : osc.regs) {
      reg = 0;
    }
  }

  OPLL_reset((OPLL*)opll);
  OPLL_resetPatch((OPLL*)opll, OPLL_VRC7_TONE);
}

void Nes_Vrc7_Apu::write_reg(uint8_t data) {
  addr = data;
}

void Nes_Vrc7_Apu::write_data(blip_time_t time, uint8_t data) {
  int type = (addr >> 4) - 1;
  int chan = addr & 15;
  if ((unsigned)type < 3 && chan < osc_count) {
    oscs[chan].regs[type] = data;
  }
  if (addr < 0x08) {
    inst[addr] = data;
  }

  if (time > next_time) {
    run_until(time);
  }

  OPLL_writeReg((OPLL*)opll, addr, data);
}

void Nes_Vrc7_Apu::end_frame(blip_time_t time) {
  if (time > next_time) {
    run_until(time);
  }

  next_time -= time;
  assert(next_time >= 0);

  for (int i = osc_count; --i >= 0;) {
    Blip_Buffer* output = oscs[i].output;
    if (output != nullptr) {
      output->set_modified();
    }
  }
}

void Nes_Vrc7_Apu::save_snapshot(vrc7_snapshot_t* out) const {
  out->latch = addr;
  out->delay = next_time;
  for (int i = osc_count; --i >= 0;) {
    for (int j = 0; j < 3; ++j) {
      out->regs[i][j] = oscs[i].regs[j];
    }
  }
  memcpy(out->inst, inst, 8);
}

void Nes_Vrc7_Apu::load_snapshot(vrc7_snapshot_t const& in) {
  assert(offsetof(vrc7_snapshot_t, delay) == 28 - 1);

  reset();
  next_time = in.delay;
  write_reg(in.latch);
  int i = 0;
  for (i = 0; i < osc_count; ++i) {
    for (int j = 0; j < 3; ++j) {
      oscs[i].regs[j] = in.regs[i][j];
    }
  }

  memcpy(inst, in.inst, 8);
  for (i = 0; i < 8; ++i) {
    OPLL_writeIO((OPLL*)opll, 0, i);
    OPLL_writeIO((OPLL*)opll, 1, in.inst[i]);
  }

  for (i = 0; i < 3; ++i) {
    for (int j = 0; j < 6; ++j) {
      OPLL_writeIO((OPLL*)opll, 0, 0x10 + i * 0x10 + j);
      OPLL_writeIO((OPLL*)opll, 1, oscs[j].regs[i]);
    }
  }
}

void Nes_Vrc7_Apu::run_until(blip_time_t end_time) {
  assert(end_time > next_time);

  blip_time_t time = next_time;
  // Blip_Buffer* const mono_output = mono.output;
  // int32_t buffer [2];
  // e_int32* buffers[2] = {&buffer[0], &buffer[1]};
  /*
  if ( mono_output )
  {
  */
  // optimal case
  do {
    int amp = OPLL_calc((OPLL*)this->opll);
    int delta = amp - mono.last_amp;
    if (delta != 0) {
      mono.last_amp = amp;
      synth.offset_inline(time, delta, mono.output);
    }
    time += period;
  } while (time < end_time);
  /*
  }
  else
  {
          mono.last_amp = 0;
          do
          {
                  OPLL_advance( (OPLL *) opll );
                  for ( int i = 0; i < osc_count; ++i )
                  {
                          Vrc7_Osc& osc = oscs [i];
                          if ( osc.output )
                          {
                                  OPLL_calc_stereo( (OPLL *) opll, buffers, 1, i );
                                  int amp = buffer [0] + buffer [1];
                                  int delta = amp - osc.last_amp;
                                  if ( delta )
                                  {
                                          osc.last_amp = amp;
                                          synth.offset( time, delta, osc.output );
                                  }
                          }
                  }
                  time += period;
          }
          while ( time < end_time );
  }
  */
  next_time = time;
}
