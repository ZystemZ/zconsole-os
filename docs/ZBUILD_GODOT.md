# ZBuild Godot no ZConsole OS

O ZBuild é o módulo do ZConsole SDK para exportar projetos Godot 4.x e gerar cartuchos ZCARD com builds nativos.

## Interface gráfica

No Z-GAME HUB, abra **Godot / ZBuild** e informe:

1. A pasta do projeto (deve conter `project.godot`).
2. O caminho do cartucho de saída.
3. O destino: `windows`, `linux`, `dual`, `android` ou `all`.
4. Opcionalmente, o caminho do executável Godot. Se vazio, o ZBuild procura Godot no PATH.

Clique em **Exportar e gerar ZCARD**. O log da ferramenta aparece na própria tela.

## Linha de comando

O comando `zbuild` é instalado no sistema:

```bash
zbuild --version
zbuild --project ~/Projetos/MeuJogo --output ~/Builds/MeuJogo.zcard --targets dual
zbuild --project ~/Projetos/MeuJogo --output ~/Builds/MeuJogo-Android.zcard --targets android
```

A CLI também pode ser chamada diretamente:

```bash
python3 /usr/share/zconsole/sdk/zbuild-godot.py --help
```

## Requisitos

- Godot 4.x no PATH ou caminho indicado.
- Export templates da mesma versão do Godot.
- Presets configurados no `export_presets.cfg`, com nomes padrão Windows Desktop, Linux/X11 e Android (ou nomes passados pela CLI).
- Android: JDK, Android SDK, preset Android em modo Gradle e assinatura configurada no ambiente do desenvolvedor.

## Compatibilidade de cartucho e execução

O builder gera um contêiner ZIP legado `.zcard`, pois é o formato de pacote já aceito pelo runtime atual. O manifesto usa `zgame-native-0.1` e lista os arquivos de cada destino em `executables`.

No desktop, o Z-GAME HUB seleciona o binário correspondente ao sistema hospedeiro e materializa arquivos complementares do mesmo diretório (por exemplo, o `.pck`) antes de iniciar o jogo.

**Não há conversão para ZEXE.** O ZEXE pertence à CPU virtual experimental; um jogo Godot exportado continua sendo um aplicativo nativo Godot e depende do runtime Godot e das bibliotecas necessárias no sistema alvo.

## Preservação e segurança

- O projeto original não é usado como diretório de exportação; a ferramenta cria uma cópia temporária sem `.git` e caches.
- O arquivo de saída deve ficar fora da pasta do projeto.
- Credenciais, keystores e senhas Android não devem ser versionados.
- O pacote é validado como ZIP e recebe manifesto e hashes SHA-256.
- A saída ZIP64 é desabilitada para evitar gerar pacotes incompatíveis com o leitor atual.

## Estado

Esta integração é a primeira versão do backend e da interface. Exportações reais dependem dos presets e templates instalados. O workflow de build do ZConsole OS deve compilar o Z-GAME HUB para validar o C++ e o QML no ambiente de destino.
