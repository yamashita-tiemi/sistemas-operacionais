# Gerenciamento de Processos e Threads no Linux

Este repositório contém implementações práticas desenvolvidas durante a disciplina **Sistemas Operacionais (DCC062)** da **Universidade Federal de Juiz de Fora (UFJF)**. O projeto tem como objetivo explorar diretamente as chamadas de sistema do Linux para realizar operações com processos e threads, sem utilizar abstrações prontas da linguagem C.

## Objetivo

O foco principal é a utilização do mecanismo de **syscall**, que permite que programas solicitem serviços diretamente ao núcleo (**kernel**) do sistema operacional.

As funcionalidades implementadas incluem:

- Criação de processos e threads
- Listagem de processos e threads ativos
- Finalização de processos e threads

> **Nota:** O uso de bibliotecas de alto nível foi evitado propositalmente, visando o entendimento direto da comunicação entre aplicações e o kernel.

## Requisitos

- Sistema operacional Linux
- Compilador GCC
- Utilitário Make

## Como Executar

1. Clone este repositório: ```git clone https://github.com/yamashita-tiemi/sistemas-operacionais.git```

2. Acesse o diretório do projeto: ```cd sistemas-operacionais```

3. Compile o programa usando make: ```make```

4. Execute o codigo gerado: ```./process_manager```
