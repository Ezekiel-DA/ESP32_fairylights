#include <FastLED.h>

#include "esp_system.h"
#include "esp_attr.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

#include <AceButton.h>
using namespace ace_button;

#include "buttons.h"

#define H1_PIN 17
#define H2_PIN 16

#define animationDelay 5


/**
 * Notes:
 *  counter mode MCPWM_UP_DOWN_COUNTER appears to halve the duty cycle of operator B? But not A? Somehow?
 *  But this doesn't actually matter if we're just setting A and B to the same duty cycle with one in active high, the other in active low?
 * 
 *  Why not 100% as the max duty cycle on PWMed animations? Because, somehow, it works the first time, but after using mcpwm_set_signal_low/mcpwm_set_signal_high, then transitioning back to PWM, 100% glitches out
 */


void setSymmetricDutyTypes() {
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, MCPWM_DUTY_MODE_0); // set output A to active high...
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, MCPWM_DUTY_MODE_1); // ... and the matching output B to active low to get a symmetric signal
}

/**
 * Animation interface. Subclasses should implement:
 *    a ctor that calls the base class ctor with an animation delay (ms to have elapsed before another update is allowed). NB: Using an animation delay of 0 (which is the default ctor) means NO ANIMATION UPDATE EVER
 *    transitionInto(): perform some setup when the animation starts; make sure do set any MCPWM options that might be needed as there is no guarantee of the previous state
 *    update(): perform one "frame" of animation; running this at the correct time is handled automatically
 * 
 * Then, somewhere in your main loop, you should call:
 *    animate(), which will check if it's time to update, and if so call your update() implementation. Do NOT re-implement animate()
 */
class IAnimation {
protected:
  uint8_t _animationDelay;
  uint16_t _prev = 0;

public:
  IAnimation(uint8_t iAnimationDelay) : _animationDelay(iAnimationDelay) {};
  IAnimation() : _animationDelay(0) {};

  virtual void update() = 0;
  virtual void transitionInto() = 0;

  virtual void animate() {
    if (_animationDelay == 0)
      return;

    uint16_t now = millis();
    if ((uint16_t)(now - _prev) >= _animationDelay) {
      this->update();
      _prev = now;
    }
  };
};

class SteadyAnimation: public IAnimation {
public:
  virtual void update() {};

  virtual void transitionInto() {
    setSymmetricDutyTypes();
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, 0.5f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, 0.5f);
  };
};

class TwinkleAnimation : public IAnimation {
private:
  uint8_t _step = 0;

public:
  TwinkleAnimation() : IAnimation(10) {};

  virtual void update() {    
    uint8_t val = scale8(cubicwave8(_step), 99); // see notes above
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A, val);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B, val);
    ++_step;
  }

  virtual void transitionInto() {
    setSymmetricDutyTypes();
    _step = 0;
    _prev = millis();
  }
};

class AlternateAnimation : public IAnimation {
private:
  bool _state = false;

public:
  AlternateAnimation() : IAnimation(250) {};

  virtual void update() {
    if (_state) {
      mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A);
      mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
    } else {
      mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
      mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A);
    }
    _state = !_state;
  }

  virtual void transitionInto() {
    _state = false;
    _prev = millis();
  }
};

class OffAnimation : public IAnimation {

public:
  virtual void update() {};

  virtual void transitionInto() {
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
  }
};

OffAnimation off;
TwinkleAnimation twinkle;
AlternateAnimation alternate;
SteadyAnimation steady;

IAnimation* modes[] = { &off, &twinkle, &alternate, &steady };

void setup() { 
  Serial.begin(115200);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, H1_PIN);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, H2_PIN);

  setupButtons();

  mcpwm_config_t pwm_config = {
        .frequency = 200,   // 200Hz seems like a good value for stable persistence of visions
        .cmpr_a = 0.0f,     // duty cycle of PWMxA
        .cmpr_b = 100.0f,   // duty cycle of PWMxB; remember that this output will be active low (to make it symmetric), so high time = 1.0 - .cmpr_b
        .duty_mode = MCPWM_DUTY_MODE_0, // active high mode
        .counter_mode = MCPWM_UP_DOWN_COUNTER,
    };

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    setSymmetricDutyTypes();
    
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_GEN_B);
}

void loop() {
  checkButtons();

  static uint8_t prevModeNumber = 0;
  if (modeNumber != prevModeNumber) {
    modes[modeNumber]->transitionInto();
    prevModeNumber = modeNumber;
  }

  modes[modeNumber]->animate();  
}
