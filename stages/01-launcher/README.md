# Etapa 01

**Objetivo:** Launcher UWP com importação exclusiva Steam (SteamKit2, login só por QR code), download do depot 758332 pinado, por jogo, com verificação por hash.

**Pronto quando:** pacote instala e baixa+verifica o jogo.

**Estado:** concluído

## Log de decisões
-
- Login só por QR code (pedido do usuário): sem campo de senha nem Steam Guard por código. O QR fica sempre na tela e é refeito sozinho quando expira ou a conexão cai.
- Tela desenhada em 960x540 (resolução efetiva do Xbox) dentro de um `Viewbox`: mesmo layout no console e no PC.
- Download por jogo: filtra o manifest pelos prefixos `sm1\` e `sm2\`. Os arquivos da raiz do depot (launcher da Steam para PC) não são baixados. Ao terminar, grava `smN\.shenmue_ok`; instalações antigas são detectadas pelo exe.

