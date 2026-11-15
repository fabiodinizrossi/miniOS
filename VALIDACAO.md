# Validação do pacote

Preparado em 14 de novembro de 2026, horário de Brasília.

## Verificado

- Compilação e ligação no host Linux x86_64 com GCC e GNU ld, usando as opções do Makefile, sem avisos e com `-Werror`.
- ELF32 i386, cabeçalho Multiboot 1 alinhado no offset 4096, checksum correto e ausência de símbolos indefinidos.
- Execução do código de entrada Assembly e do kernel em emulação x86 de 32 bits com Unicorn 2.1.4. O teste carregou os segmentos ELF e simulou o estado de entrada Multiboot.
- Caminho de inicialização do kernel, configuração da GDT e pilha, saída VGA e serial.
- Comandos, leitura das informações básicas de memória, linha vazia, comando desconhecido, limite de 127 caracteres, Backspace, rolagem e parada da CPU.
- Entrada PS/2 simulada, incluindo Shift e Caps Lock.

## Pendente de verificação no ambiente de aula

QEMU, GRUB e xorriso não estavam disponíveis no ambiente de preparação. Não foi possível instalar esses componentes. Portanto, a criação da ISO e o boot completo com firmware, GRUB e dispositivos emulados do QEMU não foram executados. O teste Unicorn usa callbacks de E/S simplificados e não substitui essa verificação.

Execute `make run-direct`, `make iso` e `make run` na máquina de referência antes de usar o material com estudantes. Teste também o teclado dentro da janela gráfica. macOS, Windows/WSL e Ubuntu 24.04 não foram testados diretamente nesta preparação.

## Repetir as verificações

A conferência estática depende apenas dos utilitários já previstos no guia:

```bash
make check
```

O teste adicional de CPU é opcional, destinado ao professor. Para evitar alterar o Python do sistema, use um ambiente virtual:

```bash
sudo apt install -y python3-venv
python3 -m venv .venv
.venv/bin/pip install unicorn==2.1.4
.venv/bin/python tools/emulator_test.py build/kernel.elf
```

Não é preciso instalar Unicorn para construir ou executar o sistema no QEMU.
