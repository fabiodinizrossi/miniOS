# MiniOS didático — do código ao primeiro boot

Guia prático para construir um kernel próprio em C e Assembly, inicializável em um PC x86 de 32 bits emulado pelo QEMU. Material pensado para Organização e Arquitetura de Computadores e introdução a Sistemas Operacionais.

## 1. Resultado e limites

Ao concluir, o estudante terá um executável ELF de kernel e poderá gerar uma ISO inicializável pelo GRUB. O kernel apresenta um console de texto, recebe entrada pelo teclado PS/2 ou pela porta serial emulada e executa `help`, `info`, `echo`, `clear`, `color` e `halt`.

Este projeto não utiliza o kernel Linux. O Linux é o ambiente de desenvolvimento. O código usa acesso direto a memória e portas de entrada e saída. GRUB é um componente externo já pronto; não estamos escrevendo nosso próprio bootloader.

A primeira versão trabalha em modo protegido de 32 bits, com segmentos planos, sem paginação e com interrupções mascaráveis desabilitadas. O teclado é lido por polling. Não há processos, escalonador, arquivos, rede, proteção entre aplicações ou execução de programas externos. Os comandos são funções do próprio kernel. É um protótipo de SO para fins didáticos.

## 2. Conhecimentos prévios e ferramentas

Conhecimentos desejáveis: funções, vetores, strings e ponteiros em C; números hexadecimais; uso básico de terminal. O professor pode fornecer a parte em Assembly inicialmente.

| Ferramenta | Função |
|---|---|
| GCC | Compilar C e montar `boot.S` com o GNU assembler |
| GNU ld | Organizar e ligar os objetos no executável |
| GNU Make | Automatizar a construção |
| Python 3 | Conferir o cabeçalho e o formato do executável |
| GRUB e grub-mkrescue | Carregar o kernel pela ISO |
| xorriso | Auxiliar na construção da ISO |
| QEMU | Emular o computador no qual o kernel roda |

Não é necessário instalar NASM. `boot.S` usa sintaxe GNU/AT&T.

## 3. Preparar o ambiente

Ambiente de referência: Ubuntu 24.04 LTS **amd64/x86_64**, em computador, máquina virtual ou WSL2. Os comandos abaixo destinam-se a esse ambiente. Não misture compiladores do macOS com os comandos de compilação deste guia.

### Linux x86_64

Abra o terminal e execute:

```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib binutils python3 \
    qemu-system-x86 grub-common grub-pc-bin xorriso unzip
uname -m
gcc --version
qemu-system-i386 --version
grub-mkrescue --version
```

`uname -m` deve mostrar `x86_64`. O GCC produzirá código de 32 bits com `-m32`, embora o Linux hospedeiro seja de 64 bits.

### Windows

Use Ubuntu 24.04 no WSL2 e execute a instalação acima dentro do Ubuntu. `make run-terminal` funciona sem janela gráfica. Para `make run`, é necessário que o ambiente tenha suporte a janelas Linux, como WSLg, ou usar uma máquina virtual Ubuntu com desktop.

### macOS, inclusive Apple Silicon

Para manter um único roteiro em sala, utilize uma máquina virtual **Ubuntu amd64/x86_64**. Em Apple Silicon ela precisa ser emulada, por exemplo em um ambiente que ofereça emulação x86_64, e será mais lenta que uma VM ARM nativa. Uma VM Ubuntu ARM64 não atende aos comandos GCC/GRUB deste guia sem adaptar a cadeia de compilação.

Sugestão para o laboratório: o professor prepara e valida uma VM padrão e distribui a mesma base aos estudantes. Uma VM com interface gráfica facilita o teste do teclado PS/2. Um host Linux x86_64 remoto permite o modo serial.

Verifique dentro do Ubuntu:

```bash
uname -m
```

Se aparecer `aarch64` ou `arm64`, não prossiga com a instalação como se fosse o ambiente de referência. Outra opção futura é montar uma cadeia `i686-elf`, mas ela fica fora deste primeiro roteiro.

## 4. Abrir o projeto

Extraia o ZIP e entre na pasta `minios`. Na pasta que contém o ZIP:

```bash
unzip MiniOS_Didatico_Completo.zip
cd minios
ls
```

Se a extração foi feita pela interface gráfica, abra um terminal na pasta `minios`. Os arquivos principais são:

| Arquivo | Responsabilidade |
|---|---|
| `src/boot.S` | Cabeçalho Multiboot, GDT, segmentos, pilha e chamada ao C |
| `src/kernel.c` | Console VGA, serial, teclado e comandos |
| `linker.ld` | Endereço de carga, entrada e organização das seções |
| `boot/grub/grub.cfg` | Entrada de menu que carrega o kernel |
| `Makefile` | Comandos reproduzíveis de construção e execução |
| `tools/check_elf.py` | Validação estática do executável |
| `CODIGOS_COMPLETOS.md` | Cópia de todos os arquivos de código para leitura |

Não há trechos a completar para fazer o primeiro boot. A construção deve ser executada como usuário comum; apenas a instalação dos pacotes usa `sudo`.

## 5. Compilar e conferir

```bash
make
make check
```

Resultado esperado da conferência:

```text
OK: ELF32 i386, Multiboot no offset 4096, sem simbolos indefinidos.
```

O offset pode variar com alterações no projeto, desde que permaneça alinhado a 4 bytes e que todo o cabeçalho esteja nos primeiros 8192 bytes. `make check` verifica o formato ELF32, a arquitetura, a assinatura Multiboot e a ausência de símbolos indefinidos. Ele não demonstra que o sistema iniciou.

Arquivos gerados:

- `build/boot.o` e `build/kernel.o`, objetos intermediários.
- `build/kernel.elf`, kernel ligado e pronto para o carregador.

Para examinar a construção:

```bash
file build/kernel.elf
readelf -h build/kernel.elf
readelf -l build/kernel.elf
objdump -d build/kernel.elf | less
nm -n build/kernel.elf | less
```

O endereço de entrada corresponde a `_start`. O kernel é ligado a partir de 1 MiB. Observe que offsets dentro do arquivo e endereços na memória são coisas diferentes.

## 6. Primeiro boot, sem precisar da ISO

```bash
make run-direct
```

Abre uma janela QEMU e carrega o ELF pelo suporte Multiboot do próprio emulador. Esse atalho permite depurar o kernel antes de incluir o GRUB no experimento. Ele não testa o caminho de inicialização pela ISO.

Saída esperada:

```text
MiniOS didatico
Kernel proprio em C e Assembly. Digite help.
minios>
```

Clique dentro da janela para digitar. O mapa de teclado do kernel é US e aceita caracteres ASCII. Acentos e layout ABNT2 não estão implementados. Para liberar a captura de teclado e mouse do QEMU, use `Ctrl+Alt+G` quando aplicável.

A saída é duplicada na serial, exibida no terminal que abriu o QEMU. No modo gráfico, prefira digitar na janela para testar o teclado emulado.

## 7. Gerar e iniciar a ISO com GRUB

```bash
make iso
make run
```

`make iso` verifica também o reconhecimento pelo GRUB e gera `build/minios.iso`. `make run` refaz as etapas necessárias e inicia a ISO pelo CD-ROM virtual.

Neste caminho, o firmware do PC emulado encontra o CD, o GRUB carrega o ELF e transfere o controle para `_start`. O Assembly prepara o ambiente e chama `kernel_main`.

A ISO foi projetada para o PC BIOS emulado usado neste guia. Não assuma suporte a boot UEFI, Secure Boot ou hardware físico. Os comandos fornecidos utilizam somente um CD virtual, sem gravar discos físicos.

## 8. Usar sem interface gráfica

```bash
make run-terminal
```

O kernel usa a serial do QEMU para entrada e saída. Digite os comandos e pressione Enter. O terminal hospedeiro pode manter a entrada em modo de linha, portanto não é o mesmo teste da leitura PS/2. Se o terminal duplicar visualmente o texto digitado, isso pode ser o eco local do terminal, somado ao eco do kernel.

Encerre o QEMU com `Ctrl+C` neste modo. `halt` para a CPU emulada, mas não fecha o processo QEMU.

## 9. Experimentar os comandos

Digite um comando por vez, em minúsculas:

```text
help
info
echo Ola, turma!
color 2
echo Texto verde
color 9
echo Texto azul claro
color 7
clear
comandoinexistente
halt
```

| Comando | Resultado |
|---|---|
| `help` | Lista as funções disponíveis |
| `info` | Mostra arquitetura, limites e informações básicas de memória do carregador |
| `echo TEXTO` | Repete a mensagem |
| `color N` | Muda a cor VGA dos próximos caracteres, com N entre 0 e 9 |
| `clear` | Limpa a tela e reposiciona a escrita |
| `halt` | Executa uma parada permanente da CPU emulada |

`color 0` produz texto preto sobre fundo preto. Use `color 7` para recuperar a visibilidade. A mudança de cor afeta a janela VGA, não as cores do terminal serial.

As informações de memória vêm dos campos básicos do Multiboot. Não são uma medição de memória livre e não substituem um mapa completo de regiões disponíveis.

O buffer aceita até 127 caracteres por linha. Caracteres adicionais são ignorados até Enter ou Backspace. Setas, edição no meio da linha e histórico não são implementados.

## 10. Entender o código em uma ordem produtiva

1. Leia `kernel_main`. Localize a inicialização, a mensagem, o buffer e o laço de entrada.
2. Leia `execute`. Relacione cada comando a uma condição e a uma função.
3. Leia `putchar_os`, `clear_screen` e `scroll_screen`. Identifique posição, cor e escrita na memória VGA.
4. Leia `inb`, `outb` e `keyboard_poll`. Diferencie porta de entrada e saída de endereço de memória.
5. Leia `boot.S`. Identifique assinatura Multiboot, descritores de segmentos, pilha e chamada C.
6. Leia `linker.ld` e `Makefile`. Reconstrua o percurso entre arquivos-fonte, objetos, ELF e ISO.

Pontos para explicar em aula:

- `0xB8000` é a região VGA de texto no PC emulado escolhido. Não é um endereço portátil para qualquer computador.
- Cada célula VGA tem um byte de caractere e um byte de atributo.
- `volatile` impede que o compilador trate os acessos VGA como operações comuns descartáveis; não é uma solução geral de sincronização.
- A GDT configura segmentos de código e dados planos. Isso não implementa memória virtual por paginação.
- A pilha permite chamadas de funções e variáveis locais. Ela possui 16 KiB fixos nesta versão.
- `-ffreestanding` indica que o programa não está num ambiente de aplicação C convencional. Não há `printf`, `malloc` ou sistema hospedeiro disponível ao kernel.
- `cli` mantém interrupções mascaráveis desabilitadas. Não existe IDT configurada. Uma exceção de CPU ainda pode ocorrer e provocar falha do sistema.
- `pause` no laço de polling não substitui escalonamento nem torna o projeto eficiente em energia. Não podemos usar `hlt` para aguardar teclado sem implementar corretamente interrupções.
- As rotinas `memcpy`, `memmove`, `memset` e `memcmp` fornecem operações básicas de memória sem depender da libc.

## 11. Atividades progressivas

### Atividade 1 — Identidade do sistema

Troque a mensagem inicial pelo nome da turma. Execute `make run-direct` e explique quais arquivos foram recompilados. Depois use `make run` para testar pela ISO.

### Atividade 2 — Novo comando

Dentro de `execute`, antes do tratamento de comando desconhecido, acrescente:

```c
    } else if (equal(line, "sobre")) {
        print("Sistema desenvolvido pela turma de Organizacao de Computadores.\n");
```

O trecho representa um novo ramo da cadeia `if/else if`; mantenha as chaves adjacentes da função. Acrescente `sobre` ao texto de `help`, recompile e teste.

### Atividade 3 — Limites do buffer

Teste uma linha vazia, um comando inválido, Backspace com a linha vazia, uma linha com 127 caracteres e uma linha maior. Explique por que `line[used] = 0` não ultrapassa o vetor.

### Atividade 4 — Rolagem e representação

Use `echo` repetidamente até a tela rolar. Localize o deslocamento das linhas no código e explique por que a última linha precisa ser limpa. Modifique a cor inicial de `0x0F` para `0x0A`.

### Atividade 5 — Consulta contínua e interrupções

Observe que o sistema consulta repetidamente as portas. Desenhe ou descreva como o fluxo mudaria se o teclado notificasse a CPU por interrupção. Não acrescente apenas `sti`: antes disso seriam necessários IDT, tratadores, configuração do controlador de interrupções e preservação de contexto.

## 12. Sequência sugerida para oito encontros

Cada encontro pode ocupar dois períodos, com ajuste conforme o domínio de C da turma.

| Encontro | Foco | Evidência de aprendizagem |
|---|---|---|
| 1 | Preparação e primeiro boot | Sistema rodando e relato do caminho de inicialização |
| 2 | Fontes, objetos e ELF | Identificação da entrada e das seções |
| 3 | Console e memória VGA | Mensagem e cor personalizadas |
| 4 | Teclado e portas | Explicação do polling e teste de entrada |
| 5 | Interpretador | Novo comando implementado |
| 6 | Buffers e rolagem | Registro dos testes de limite |
| 7 | Projeto em grupo | Funcionalidade adicional integrada |
| 8 | Demonstração e explicação | Alteração simples feita ao vivo e discussão dos limites |

Uma extensão adequada é um comando que converte um inteiro decimal para hexadecimal, com validação de entrada e detecção de overflow. Interrupções, temporizador, alocação de memória e processos formam um segundo módulo, com maior complexidade.

Avaliação sugerida: funcionamento e testes 30%; explicação da arquitetura e do boot 30%; qualidade da implementação 25%; documentação e apresentação 15%. Não pontue somente a reprodução do código fornecido.

## 13. Resolver problemas comuns

| Sintoma | Verificação e ação |
|---|---|
| `gcc` ou `make` não encontrado | Instale `build-essential` no Ubuntu |
| GCC não reconhece `-m32` ou gera arquitetura errada | Confira `uname -m`; use o ambiente Ubuntu amd64 de referência |
| Falha envolvendo suporte a 32 bits | Confira a instalação de `gcc-multilib` |
| `grub-mkrescue` não encontrado | Instale `grub-common`, `grub-pc-bin` e `xorriso` |
| GRUB não encontra módulos `i386-pc` | Confira `grub-pc-bin`; não é necessário instalar GRUB no disco da máquina |
| Erro de Multiboot | Execute `make clean` e `make check`; confira o script de ligação |
| QEMU não consegue abrir janela | Use `make run-terminal` ou configure um ambiente gráfico |
| Tela preta depois de `color 0` | Digite `color 7` e Enter |
| Teclado da janela não responde | Clique na janela e confira o uso do alvo gráfico; teste a serial para separar problemas |
| Pontuação diferente do teclado físico | O mapa interno é US; ABNT2 não foi implementado |
| VM ARM no Mac não compila | Use uma VM amd64 emulada ou prepare uma cadeia cruzada específica |
| Sistema trava após uma alteração | Volte à última versão funcional; erros de ponteiro ou exceções podem impedir qualquer mensagem |
| `halt` não fecha o emulador | Esse comportamento é esperado; feche a janela ou pressione `Ctrl+C` no modo terminal |

Para reconstruir tudo:

```bash
make clean
make check
make iso
```

## 14. Conferência antes de usar em aula

Execute os dois caminhos de boot, direto e pela ISO. Teste comandos válidos e inválidos, entrada vazia, Backspace, rolagem, Shift, Caps Lock e limite da linha. Teste especificamente o caminho PS/2 na janela, pois o modo serial não verifica o driver de teclado.

Registre as versões de GCC, ld, GRUB, QEMU e Ubuntu usadas no laboratório. A portabilidade deve ser comprovada na imagem padrão da turma antes da atividade.

O arquivo `VALIDACAO.md` informa exatamente quais verificações foram realizadas durante a preparação deste pacote.

## 15. Referências técnicas oficiais

- Multiboot 1 e contrato entre carregador e kernel: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
- Manual do GNU GRUB: https://www.gnu.org/software/grub/manual/grub/grub.html
- QEMU e carregamento de kernel Multiboot: https://www.qemu.org/docs/master/system/invocation.html
- GCC, ambientes freestanding: https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html
- GNU ld e scripts de ligação: https://sourceware.org/binutils/docs/ld/Scripts.html

Consulte as referências para aprofundamento. Os arquivos completos estão no próprio pacote e em `CODIGOS_COMPLETOS.md`.
