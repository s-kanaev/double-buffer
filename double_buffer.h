#include <atomic>

#pragma once

template <typename T>
class DoubleBufferReader {
public:
    T read() {
      T ret = *consume();
      consume_done();
      return ret;
    }

protected:
    virtual const T * const consume() = 0;
    virtual void consume_done() = 0;
};

template <typename T>
class DoubleBufferWriter {
public:
    void write(T &v) {
      *generate() = v;
      generate_done();
    }

protected:
    virtual T *generate() = 0;
    virtual void generate_done() = 0;
};

template <typename T>
class DoubleBuffer : public DoubleBufferReader<T>, public DoubleBufferWriter<T> {
public:
    using DataT = T;

    DoubleBuffer() : _state{0} {}

protected:
    DataT *generate() override {
      _apply_state_mask(GENERATOR_NOT_DONE_MASK);

      return &_get_inactive();
    }

    void generate_done() override {
      _clear_state_mask(GENERATOR_NOT_DONE_MASK);
      _flip();
    }

    const DataT * const consume() override {
      _apply_state_mask(CONSUMER_NOT_DONE_MASK);

      return &_get_active();
    }

    void consume_done() override {
      _clear_state_mask(CONSUMER_NOT_DONE_MASK);

      if (_state.load() & SHOULD_FLIP_MASK) {
        _flip();
      }
    }

private:
    static constexpr int BUFFER_COUNT = 2;

    DataT _buffer[BUFFER_COUNT];

    // bit 7 = consumer not done
    // bit 6 = generator not done
    // bit 5 = should flip
    // bit 0 = active idx
    static constexpr uint16_t CONSUMER_NOT_DONE_MASK = 1 << 7;
    static constexpr uint16_t GENERATOR_NOT_DONE_MASK = 1 << 6;
    static constexpr uint16_t SHOULD_FLIP_MASK = 1 << 5;
    static constexpr uint16_t ACTIVE_IDX_MASK = 1 << 0;
    std::atomic<uint16_t> _state;

    DataT &_get_active() {
      return _buffer[_state.load() & ACTIVE_IDX_MASK];
    }

    DataT &_get_inactive() {
      return _buffer[(_state.load() & ACTIVE_IDX_MASK) ^ 0x01];
    }

    void _apply_state_mask(uint16_t or_mask) {
      uint16_t state = _state.load();
      uint16_t desired = state | or_mask;

      while (!_state.compare_exchange_weak(state, desired)) {
        desired = state | or_mask;
      }
    }

    void _clear_state_mask(uint16_t clr_mask) {
      uint16_t state = _state.load();
      uint16_t desired = state & (~clr_mask);

      while (!_state.compare_exchange_weak(state, desired)) {
        desired = state & (~clr_mask);
      }
    }

    void _flip() {
      uint16_t state = _state.load();
      uint16_t desired;

      do {
        if (state & (CONSUMER_NOT_DONE_MASK | GENERATOR_NOT_DONE_MASK)) {
          _apply_state_mask(SHOULD_FLIP_MASK);
          break;
        } else {
          desired = state ^ ACTIVE_IDX_MASK;
          desired &= ~SHOULD_FLIP_MASK;

          _state.compare_exchange_strong(state, desired);

          break;
        }
      } while (state & (CONSUMER_NOT_DONE_MASK | GENERATOR_NOT_DONE_MASK));
    }
};
