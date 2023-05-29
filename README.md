# ProjetoBLE

# Projeto para a disciplina Sistemas Embarcados.



- Periférico que possui:
    - Uma característica com permissão de escrita e que recebe os dados
    - Tem uma característica sem permissão e que realiza notificação
        - A notificação é enviada sempre que um dado for recebido
        - O conteúdo da notificação é o dado recebido com as letras minúsculas convertidas para maiúsculas

- Central que:
    - Procura e se conecta ao periférico
    - Envia ao periférico o que o usuário escreve no terminal
    - Imprime a resposta enviada pelo periférico
