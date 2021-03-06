#include "odrive_main.h"

#include "low_level.h"

ThermistorCurrentLimiter::ThermistorCurrentLimiter(uint16_t adc_channel,
                                                   const float* const coefficients,
                                                   size_t num_coeffs,
                                                   const float& temp_limit_lower,
                                                   const float& temp_limit_upper,
                                                   const bool& enabled) :
    adc_channel_(adc_channel),
    coefficients_(coefficients),
    num_coeffs_(num_coeffs),
    temperature_(NAN),
    temp_limit_lower_(temp_limit_lower),
    temp_limit_upper_(temp_limit_upper),
    enabled_(enabled)
{
}

void ThermistorCurrentLimiter::update() {
    const float voltage = get_adc_voltage_channel(adc_channel_);
    const float normalized_voltage = voltage / adc_ref_voltage;
    temperature_ = horner_fma(normalized_voltage, coefficients_, num_coeffs_);
}

bool ThermistorCurrentLimiter::do_checks() {
    if (enabled_ && temperature_ >= temp_limit_upper_ + 5) {
        return false;
    }
    return true;
}

float ThermistorCurrentLimiter::get_current_limit(float base_current_lim) const {
    if (!enabled_) {
        return base_current_lim;
    }

    const float temp_margin = temp_limit_upper_ - temperature_;
    const float derating_range = temp_limit_upper_ - temp_limit_lower_;
    float thermal_current_lim = base_current_lim * (temp_margin / derating_range);
    if (!(thermal_current_lim >= 0.0f)) { // Funny polarity to also catch NaN
        thermal_current_lim = 0.0f;
    }

    return std::min(thermal_current_lim, base_current_lim);
}

OnboardThermistorCurrentLimiter::OnboardThermistorCurrentLimiter(uint16_t adc_channel, const float* const coefficients, size_t num_coeffs) :
    ThermistorCurrentLimiter(adc_channel,
                             coefficients,
                             num_coeffs,
                             config_.temp_limit_lower,
                             config_.temp_limit_upper,
                             config_.enabled)
{
}

OffboardThermistorCurrentLimiter::OffboardThermistorCurrentLimiter() :
    ThermistorCurrentLimiter(UINT16_MAX,
                             &config_.thermistor_poly_coeffs[0],
                             num_coeffs_,
                             config_.temp_limit_lower,
                             config_.temp_limit_upper,
                             config_.enabled)
{
    decode_pin();
}

bool OffboardThermistorCurrentLimiter::apply_config() {
    config_.parent = this;
    decode_pin();
    return true;
}

void OffboardThermistorCurrentLimiter::decode_pin() {
    adc_channel_ = channel_from_gpio(get_gpio(config_.gpio_pin));
}
