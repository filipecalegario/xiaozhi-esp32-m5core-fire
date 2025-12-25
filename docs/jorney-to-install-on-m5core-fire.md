# Journey to Install XiaoZhi on M5Stack Core Fire

**Data:** 25 de Dezembro de 2024 (Atualizado)

## Status Atual: FUNCIONANDO COM ÁUDIO FULL-DUPLEX

O M5Stack Core Fire agora funciona com XiaoZhi usando o **Node Base** (WM8978):

| Componente | Status | Notas |
|------------|--------|-------|
| Display | Funcionando | ILI9342C LCD 320x240 |
| LED Strip | Desabilitado | Node Base não tem LEDs |
| Botões | Funcionando | A=escuta, B=vol-, C=vol+ |
| Bateria | Opcional | Requer M5GO Base bottom |
| **Audio Output** | **FUNCIONANDO** | WM8978 I2S full-duplex |
| **Audio Input** | **FUNCIONANDO** | WM8978 I2S full-duplex |

## Evolução da Solução

### Fase 1: M5GO Base (Solução Antiga - DEPRECATED)

A primeira tentativa usou o M5GO Base que vem com o Core Fire:
- Microfone: ADC oneshot mode (GPIO34)
- Speaker: PDM output (GPIO25)
- **Problema**: Não suportava áudio full-duplex (entrada E saída simultâneas)
- **Limitação**: Respostas apenas em texto no display

### Fase 2: Node Base (Solução Atual)

Devido a problemas de hardware com o M5GO Base (desligamentos ao conectar), migramos para o **M5Stack Node Base** com codec WM8978:

- **Full-duplex I2S**: Entrada e saída simultâneas
- **Qualidade superior**: 24-bit codec vs 12-bit ADC
- **Menor latência**: Sem necessidade de ADC oneshot polling

## Hardware Necessário

- M5Stack Core Fire (2018.2A)
- **M5Stack Node Base** (com WM8978 codec)
- Cabo USB-A para USB-C (USB-C to USB-C NÃO funciona no 2018.2A)

## Desafios Resolvidos

### 1. MCLK no GPIO0 (Strapping Pin)
- GPIO0 é usado para seleção de boot
- Solução: WM8978 configura MCLK após boot completo

### 2. Stack Overflow na Task de Áudio
- Buffers alocados na stack causavam overflow
- Solução: Usar `heap_caps_malloc()` com buffers DMA-capable

### 3. Heap Fragmentation em Streaming Longo
- malloc/free repetidos causavam fragmentação
- Solução: Buffers pré-alocados no construtor

### 4. Áudio Para Após 20-30 Segundos
- Desabilitar RX channel parava o clock compartilhado
- Solução: Manter canais I2S sempre habilitados (full-duplex clock sharing)

## Documentação Técnica

- [docs/m5core-fire.md](m5core-fire.md) - Documentação completa da implementação atual
- [main/boards/m5stack-core-fire/README.md](../main/boards/m5stack-core-fire/README.md) - Documentação da board

### Documentação Antiga (M5GO Base)
- [m5core-fire-microphone-fix.md](m5core-fire-microphone-fix.md) - Solução antiga com ADC (DEPRECATED)

## Como Usar

1. **Botão A (clique)**: Ativar modo de escuta (listening)
2. **Botão A (longo)**: Entrar no modo de configuração WiFi
3. **Botão B (clique)**: Volume -10%
4. **Botão B (duplo clique)**: Enviar mensagem "Olá"
5. **Botão C (clique)**: Volume +10%

## Build e Flash

```bash
idf.py set-target esp32
idf.py menuconfig  # Xiaozhi Assistant -> Board Type -> M5Stack Core Fire 2018.2A
idf.py build
idf.py flash monitor
```

## Referências

- [M5Stack Node Base](https://docs.m5stack.com/en/base/node_base)
- [WM8978 Datasheet](https://www.mouser.com/datasheet/2/76/WM8978_v4.5-1141768.pdf)
- [M5Stack Core Fire](https://docs.m5stack.com/en/core/fire)
