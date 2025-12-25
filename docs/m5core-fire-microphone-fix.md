# M5Stack Core Fire - Microphone Fix Documentation

**Data:** 24 de Dezembro de 2024

OBSERVAÇÃO: ESSA É UMA VERSÃO ANTIGA DA SOLUÇÃO.

## Resumo

Este documento descreve a solução implementada para fazer o microfone MEMS BSE3729 do M5Stack Core Fire funcionar com o projeto XiaoZhi. A solução utiliza **ADC oneshot mode** em vez do modo contínuo, que não funciona corretamente neste hardware.

## O Problema

### Hardware do M5Stack Core Fire

- **Microfone:** MEMS BSE3729 conectado ao GPIO34 (ADC1_CH6)
- **Localização:** O microfone está na **M5GO Base**, não na unidade principal
- **Tipo de sinal:** Analógico (requer ADC para leitura)

### Problemas Encontrados

1. **ADC Continuous Mode não funciona**
   - O esp_codec_dev com ADC continuous mode retornava apenas zeros
   - Mesmo com DMA configurado corretamente, os valores eram sempre -16380 (sem variação)
   - Testes diretos com ADC oneshot mostraram que o hardware estava funcionando (valores 1639-2794)

2. **Conflito com LED Strip (SK6812)**
   - O LED strip usa o periférico RMT
   - Quando ativo, causava flickering na tela
   - Solução: Desabilitar o LED strip e usar `NoLed` class

3. **Taxa de amostragem incompatível**
   - O silk_resampler só suporta taxas específicas: **8000, 12000, 16000, 24000, 48000 Hz**
   - Taxa de 20kHz causava `celt_assert(0)` e reset do dispositivo
   - Solução: Usar 24kHz (ou 16kHz)

## A Solução

### 1. FireAudioCodec com ADC Oneshot

Reescrevemos completamente o `FireAudioCodec` para usar ADC oneshot mode:

```cpp
// fire_audio_codec.h
class FireAudioCodec : public AudioCodec {
private:
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_channel_t adc_channel_;
    int dc_offset_ = 2048;  // DC offset para centralizar áudio

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
    // ...
};
```

### 2. Calibração de DC Offset

O microfone MEMS produz sinal analógico centrado em ~2048 (metade de 12 bits). Calibramos automaticamente:

```cpp
// Calibrar DC offset lendo amostras iniciais
int32_t offset_sum = 0;
for (int i = 0; i < 100; i++) {
    int raw = 0;
    adc_oneshot_read(adc_handle_, adc_channel_, &raw);
    offset_sum += raw;
}
dc_offset_ = offset_sum / 100;
```

### 3. Conversão de 12-bit para 16-bit

```cpp
// Converter ADC 12-bit (0-4095) para áudio 16-bit signed (-32768 a 32767)
int32_t centered = raw - dc_offset_;
dest[i] = (int16_t)(centered * 16);  // Escalar de 12-bit para 16-bit
```

### 4. LED Strip Desabilitado

Para evitar conflitos de recursos:

```cpp
// Em m5stack_core_fire.cc
virtual Led* GetLed() override {
    static NoLed no_led;  // Retornar NoLed em vez de nullptr
    return &no_led;
}

// InitializeLedStrip() comentado no construtor
```

## Arquivos Modificados

| Arquivo | Descrição |
|---------|-----------|
| `main/boards/m5stack-core-fire/fire_audio_codec.h` | Header com ADC oneshot handle |
| `main/boards/m5stack-core-fire/fire_audio_codec.cc` | Implementação completa com ADC oneshot |
| `main/boards/m5stack-core-fire/m5stack_core_fire.cc` | LED strip desabilitado, GetLed() retorna NoLed |

## Configuração de Hardware

### config.h (valores relevantes)

```cpp
#define AUDIO_INPUT_SAMPLE_RATE     16000    // Taxa suportada pelo resampler
#define AUDIO_OUTPUT_SAMPLE_RATE    24000    // Taxa de saída
#define AUDIO_ADC_MIC_CHANNEL       ADC1_CHANNEL_6  // GPIO34
```

## Limitações

1. **Sem saída de áudio**: O M5Stack Core Fire não pode usar ADC (microfone) e DAC (speaker) simultaneamente no modo contínuo, pois ambos compartilham I2S0. A solução atual desabilita a saída de áudio, mostrando respostas em texto no display.

2. **Taxa de amostragem oneshot**: O modo oneshot é mais lento que continuous mode, mas é confiável neste hardware.

3. **Qualidade de áudio**: O ADC do ESP32 não é ideal para áudio, mas funciona adequadamente para reconhecimento de voz.

## Testes Realizados

1. **Teste ADC direto**: Valores variando (1639-2794) confirmaram hardware funcional
2. **Debug de áudio**: Logs mostrando `Audio: min=-3200 max=2400 avg=-100 range=5600`
3. **Detecção de voz**: Dispositivo detecta fala e entra no modo de processamento

## Problemas Conhecidos

- O WiFi pode precisar ser reconfigurado após flash inicial
- A M5GO Base precisa estar conectada para o microfone funcionar

## Para Fazer (Opcional)

- [ ] Remover logs de debug do `fire_audio_codec.cc` após validação completa
- [ ] Testar com diferentes níveis de volume ambiente
- [ ] Adicionar suporte a áudio output via I2S externo (futuro)

## Referências

- [ESP32 ADC Oneshot Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc_oneshot.html)
- [M5Stack Core Fire Documentation](https://docs.m5stack.com/en/core/fire)
- [MEMS BSE3729 Microphone](https://docs.m5stack.com/en/core/fire) - GPIO34 (ADC1_CH6)
