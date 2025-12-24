# Journey to Install XiaoZhi on M5Stack Core Fire

**Data:** 24 de Dezembro de 2024

## Status Atual: FUNCIONANDO

O M5Stack Core Fire agora funciona com XiaoZhi com as seguintes capacidades:

| Componente | Status | Notas |
|------------|--------|-------|
| Display | Funcionando | ILI9342C LCD 320x240 |
| LED Strip | Desabilitado | Conflito com RMT, usando NoLed |
| Botões | Funcionando | A, B, C buttons |
| Bateria | Funcionando | IP5306 power management |
| Audio Output | Desabilitado | Respostas em texto no display |
| **Audio Input** | **FUNCIONANDO** | ADC oneshot mode no GPIO34 |

## Resumo da Jornada

### Problema Original

O ESP32 original não pode usar simultaneamente:
- ADC continuous mode (microfone) - requer I2S0 para DMA
- DAC continuous mode (speaker) - também requer I2S0 para DMA

### Solução Implementada

1. **ADC Oneshot Mode**: Reescrevemos `FireAudioCodec` para usar ADC oneshot em vez de continuous mode
2. **LED Strip Desabilitado**: O periférico RMT causava conflitos, desabilitamos e usamos `NoLed`
3. **Taxa de Amostragem Corrigida**: silk_resampler só suporta 8000, 12000, 16000, 24000, 48000 Hz
4. **Calibração DC Offset**: Removemos o offset DC do sinal do microfone MEMS

### Documentação Técnica

Veja [m5core-fire-microphone-fix.md](./m5core-fire-microphone-fix.md) para detalhes técnicos completos.

## Hardware Necessário

- M5Stack Core Fire
- **M5GO Base** (contém o microfone MEMS BSE3729)

## Como Usar

1. Pressione **Botão A** para ativar modo de escuta (listening)
2. Fale no microfone (na M5GO Base)
3. A resposta aparece em texto no display

## Para Áudio Bidirecional (Futuro)

Para habilitar entrada e saída de áudio simultâneas, seria necessário adicionar um codec I2S externo (como MAX98357 + INMP441).
