// Internal stuff here to keep public header uncluttered

#pragma once

#include <cassert>

using blip_resampled_time_t = unsigned int;

#ifndef BLIP_MAX_QUALITY
#define BLIP_MAX_QUALITY 32
#endif

#ifndef BLIP_BUFFER_ACCURACY
#define BLIP_BUFFER_ACCURACY 16
#endif

#ifndef BLIP_PHASE_BITS
#define BLIP_PHASE_BITS 6
#endif

class blip_eq_t;
class Blip_Buffer;

#if BLIP_BUFFER_FAST
// linear interpolation needs 8 bits
#undef BLIP_PHASE_BITS
#define BLIP_PHASE_BITS 8

#undef BLIP_MAX_QUALITY
#define BLIP_MAX_QUALITY 2
#endif

int const blip_res = 1 << BLIP_PHASE_BITS;
int const blip_buffer_extra_ = BLIP_MAX_QUALITY + 2;

class Blip_Buffer_ {
 public:
  // Writer

  using clocks_t = int;

  // Properties of fixed-point sample position
  using fixed_t = unsigned int;                // unsigned for more range, optimized shifts
  enum { fixed_bits = BLIP_BUFFER_ACCURACY };  // bits in fraction
  enum { fixed_unit = 1 << fixed_bits };       // 1.0 samples

  // Converts clock count to fixed-point sample position
  [[nodiscard]] fixed_t to_fixed(clocks_t t) const {
    return t * factor_ + offset_;
  }

  // Deltas in buffer are fixed-point with this many fraction bits.
  // Less than 16 for extra range.
  enum { delta_bits = 14 };

  // Pointer to first committed delta sample
  using delta_t = int;

  // Pointer to delta corresponding to fixed-point sample position
  delta_t* delta_at(fixed_t /*f*/);

  // Reader

  delta_t* read_pos() {
    return buffer_;
  }

  void clear_modified() {
    modified_ = false;
  }
  [[nodiscard]] int highpass_shift() const {
    return bass_shift_;
  }
  [[nodiscard]] int integrator() const {
    return reader_accum_;
  }
  void set_integrator(int n) {
    reader_accum_ = n;
  }

  // friend class Tracked_Blip_Buffer; private:
  [[nodiscard]] bool modified() const {
    return modified_;
  }
  void remove_silence(int count);

 private:
  unsigned factor_;
  fixed_t offset_;
  delta_t* buffer_center_;
  int buffer_size_;
  int reader_accum_;
  int bass_shift_;
  delta_t* buffer_;
  int sample_rate_;
  int clock_rate_;
  int bass_freq_;
  int length_;
  bool modified_;

  friend class Blip_Buffer;
};

class Blip_Synth_Fast_ {
 public:
  int delta_factor{0};
  int last_amp{0};
  Blip_Buffer* buf{nullptr};

  void volume_unit(double /*new_unit*/);
  void treble_eq(blip_eq_t const& /*unused*/) {
  }
  Blip_Synth_Fast_();
};

class Blip_Synth_ {
 public:
  int delta_factor;
  int last_amp;
  Blip_Buffer* buf;

  void volume_unit(double /*new_unit*/);
  void treble_eq(blip_eq_t const& /*eq*/);
  Blip_Synth_(short phases[], int width);

 private:
  double volume_unit_;
  short* const phases;
  int const width;
  int kernel_unit;

  void adjust_impulse();
  void rescale_kernel(int shift);
  [[nodiscard]] int impulses_size() const {
    return blip_res / 2 * width;
  }
};

class blip_buffer_state_t {
  blip_resampled_time_t offset_;
  int reader_accum_;
  int buf[blip_buffer_extra_];
  friend class Blip_Buffer;
};

inline Blip_Buffer_::delta_t* Blip_Buffer_::delta_at(fixed_t f) {
  assert((f >> fixed_bits) < (unsigned)buffer_size_);
  return buffer_center_ + (f >> fixed_bits);
}

