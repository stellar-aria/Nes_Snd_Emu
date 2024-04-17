// Multi-channel sound buffer interface, and basic mono and stereo buffers
#pragma once

#include <system_error>
#include "Blip_Buffer.h"


// Interface to one or more Blip_Buffers mapped to one or more channels
// consisting of left, center, and right buffers.
class Multi_Buffer {
 public:
  // 1=mono, 2=stereo
  Multi_Buffer(int samples_per_frame);
  virtual ~Multi_Buffer() = default;

  // Sets the number of channels available and optionally their types
  // (type information used by Effects_Buffer)
  enum { type_index_mask = 0xFF };
  enum { wave_type = 0x100, noise_type = 0x200, mixed_type = wave_type | noise_type };
  virtual std::error_condition set_channel_count(int /*n*/, int const types[] = nullptr);
  [[nodiscard]] int channel_count() const {
    return channel_count_;
  }

  // Gets indexed channel, from 0 to channel_count()-1
  struct channel_t {
    Blip_Buffer* center;
    Blip_Buffer* left;
    Blip_Buffer* right;
  };
  virtual channel_t channel(int index);

  // Number of samples per output frame (1 = mono, 2 = stereo)
  [[nodiscard]] int samples_per_frame() const;

  // Count of changes to channel configuration. Incremented whenever
  // a change is made to any of the Blip_Buffers for any channel.
  unsigned channels_changed_count() const {
    return channels_changed_count_;
  }

  // See Blip_Buffer.h
  virtual std::error_condition set_sample_rate(int rate, int msec = blip_default_length);
  [[nodiscard]] int sample_rate() const;
  [[nodiscard]] int length() const;
  virtual void clock_rate(int /*unused*/);
  virtual void bass_freq(int /*unused*/);
  virtual void clear();
  virtual void end_frame(blip_time_t /*unused*/);
  virtual int read_samples(blip_sample_t /*unused*/[], int /*unused*/);
  [[nodiscard]] virtual int samples_avail() const;

 private:
  // noncopyable
  Multi_Buffer(const Multi_Buffer&) = delete;
  Multi_Buffer& operator=(const Multi_Buffer&) = delete;

  // Implementation
 public:
  void disable_immediate_removal() {
    immediate_removal_ = false;
  }

 protected:
  [[nodiscard]] bool immediate_removal() const {
    return immediate_removal_;
  }
  [[nodiscard]] int const* channel_types() const {
    return channel_types_;
  }
  void channels_changed() {
    channels_changed_count_++;
  }

 private:
  unsigned channels_changed_count_;
  int sample_rate_;
  int length_;
  int channel_count_;
  int const samples_per_frame_;
  int const* channel_types_;
  bool immediate_removal_;
};

// Uses a single buffer and outputs mono samples.
class Mono_Buffer : public Multi_Buffer {
 public:
  // Buffer used for all channels
  Blip_Buffer* center() {
    return &buf;
  }

  // Implementation

  Mono_Buffer();
  ~Mono_Buffer() override;
  std::error_condition set_sample_rate(int rate, int msec = blip_default_length) override;
  void clock_rate(int rate) override {
    buf.clock_rate(rate);
  }
  void bass_freq(int freq) override {
    buf.bass_freq(freq);
  }
  void clear() override {
    buf.clear();
  }
  [[nodiscard]] int samples_avail() const override {
    return buf.samples_avail();
  }
  int read_samples(blip_sample_t p[], int s) override {
    return buf.read_samples(p, s);
  }
  channel_t channel(int /*index*/) override {
    return chan;
  }
  void end_frame(blip_time_t t) override {
    buf.end_frame(t);
  }

 private:
  Blip_Buffer buf;
  channel_t chan{};
};

class Tracked_Blip_Buffer : public Blip_Buffer {
 public:
  // Non-zero if buffer still has non-silent samples in it. Requires that you call
  // set_modified() appropriately.
  [[nodiscard]] unsigned non_silent() const;

  // remove_samples( samples_avail() )
  void remove_all_samples();

  // Implementation

  int read_samples(blip_sample_t /*out*/[], int /*count*/);
  void remove_silence(int /*n*/);
  void remove_samples(int /*n*/);
  Tracked_Blip_Buffer();
  void clear();
  void end_frame(blip_time_t /*t*/);

 private:
  int last_non_silence{0};

  [[nodiscard]] delta_t unsettled() const {
    return integrator() >> delta_bits;
  }
  void remove_(int /*n*/);
};

class Stereo_Mixer {
 public:
  Tracked_Blip_Buffer* bufs[3]{};
  int samples_read{0};

  Stereo_Mixer() {
  }
  void read_pairs(blip_sample_t out[], int count);

 private:
  void mix_mono(blip_sample_t out[], int pair_count);
  void mix_stereo(blip_sample_t out[], int pair_count);
};

// Uses three buffers (one for center) and outputs stereo sample pairs.
class Stereo_Buffer : public Multi_Buffer {
 public:
  // Buffers used for all channels
  Blip_Buffer* center() {
    return &bufs[2];
  }
  Blip_Buffer* left() {
    return &bufs[0];
  }
  Blip_Buffer* right() {
    return &bufs[1];
  }

  // Implementation

  Stereo_Buffer();
  ~Stereo_Buffer() override;
  std::error_condition set_sample_rate(int /*rate*/, int msec = blip_default_length) override;
  void clock_rate(int /*rate*/) override;
  void bass_freq(int /*bass*/) override;
  void clear() override;
  channel_t channel(int /*index*/) override {
    return chan;
  }
  void end_frame(blip_time_t /*time*/) override;
  [[nodiscard]] int samples_avail() const override {
    return (bufs[0].samples_avail() - mixer.samples_read) * 2;
  }
  int read_samples(blip_sample_t /*out*/[], int /*out_size*/) override;

 private:
  enum { bufs_size = 3 };
  using buf_t = Tracked_Blip_Buffer;
  buf_t bufs[bufs_size];
  Stereo_Mixer mixer;
  channel_t chan{};
  int samples_avail_{};
};

// Silent_Buffer generates no samples, useful where no sound is wanted
class Silent_Buffer : public Multi_Buffer {
  channel_t chan{};

 public:
  Silent_Buffer();
  std::error_condition set_sample_rate(int rate, int msec = blip_default_length) override;
  void clock_rate(int /*unused*/) override {
  }
  void bass_freq(int /*unused*/) override {
  }
  void clear() override {
  }
  channel_t channel(int /*index*/) override {
    return chan;
  }
  void end_frame(blip_time_t /*unused*/) override {
  }
  [[nodiscard]] int samples_avail() const override {
    return 0;
  }
  int read_samples(blip_sample_t /*unused*/[], int /*unused*/) override {
    return 0;
  }
};

inline std::error_condition Multi_Buffer::set_sample_rate(int rate, int msec) {
  sample_rate_ = rate;
  length_ = msec;
  return {};
}

inline int Multi_Buffer::samples_per_frame() const {
  return samples_per_frame_;
}
inline int Multi_Buffer::sample_rate() const {
  return sample_rate_;
}
inline int Multi_Buffer::length() const {
  return length_;
}
inline void Multi_Buffer::clock_rate(int /*unused*/) {
}
inline void Multi_Buffer::bass_freq(int /*unused*/) {
}
inline void Multi_Buffer::clear() {
}
inline void Multi_Buffer::end_frame(blip_time_t /*unused*/) {
}
inline int Multi_Buffer::read_samples(blip_sample_t /*unused*/[], int /*unused*/) {
  return 0;
}
inline int Multi_Buffer::samples_avail() const {
  return 0;
}

inline std::error_condition Multi_Buffer::set_channel_count(int n, int const types[]) {
  channel_count_ = n;
  channel_types_ = types;
  return {};
}

inline std::error_condition Silent_Buffer::set_sample_rate(int rate, int msec) {
  return Multi_Buffer::set_sample_rate(rate, msec);
}
