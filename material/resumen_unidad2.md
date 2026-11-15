# Resumen Unidad II — Sistemas Distribuidos (semanas 5 a 8)

> Fuente: `Unidad2_SD_Chat.pptx`. Vamos en la **semana 7**, así que el control llega hasta la semana 7.
> La semana 8 (Raft) va al final como adelanto: probablemente **no** entra todavía.

---

## Semana 5 — Tiempo y relojes lógicos

### El problema del tiempo
- No existe un reloj global. Cada nodo tiene su propio reloj físico.
- **Clock drift**: los relojes de cuarzo avanzan a velocidades distintas (cambian con la temperatura), del orden de **1 s cada 11–12 días**.
- Conclusión: el tiempo absoluto no sirve; **solo importa el orden** de los eventos.

### Happened-before (→)
A → B si se cumple alguna de estas:
1. **Mismo proceso**: A ocurre antes que B en el mismo nodo.
2. **Mensaje**: A es el envío y B la recepción del mismo mensaje.
3. **Transitividad**: A → B y B → C ⇒ A → C.

Si no se cumple ninguna, A y B son **concurrentes (A || B)**.

### Reloj de Lamport
Cada proceso tiene un contador `L`, que parte en 0.
| Evento | Regla |
|---|---|
| Local | `L = L + 1` |
| Envío | `L = L + 1; send(msg, L)` |
| Recepción de T | `L = max(L, T) + 1` |

- Garantía: **A → B ⇒ C(A) < C(B)**.
- **El inverso NO vale**: que C(A) < C(B) no implica A → B (pueden ser concurrentes).
- **Talón de Aquiles**: Lamport **no detecta concurrencia**.

### Relojes vectoriales
Cada proceso i guarda un vector `V[1..N]`, que parte en [0,…,0].
- `V[i]`: cuántos eventos ejecutó i.
- `V[j]` (j ≠ i): cuántos eventos de j **sabe** i que ocurrieron.

| Evento (en el proceso i) | Regla |
|---|---|
| Local | `V[i]++` |
| Envío | `V[i]++; send(msg, V)` |
| Recepción de W | `V[i]++;` y para todo j ≠ i: `V[j] = max(V[j], W[j])` |

**Comparación de vectores**
- `V1 ≤ V2` ⟺ `V1[k] ≤ V2[k]` para todo k
- `V1 < V2` ⟺ `V1 ≤ V2` y `V1 ≠ V2`
- `V1 || V2` ⟺ ni `V1 < V2` ni `V2 < V1`

Garantía **bidireccional**: **A → B ⟺ VC(A) < VC(B)**.

### ¿Cuándo usar cada uno?
| | Lamport | Vector Clocks |
|---|---|---|
| Sirve para | orden total | detectar concurrencia / conflictos |
| Costo | 1 entero | N enteros por mensaje |
| Ejemplos | logs de eventos | Git, CRDTs, Amazon Dynamo |

- **Trade-off**: precisión vs. overhead.
- **CRDT** (Conflict-free Replicated Data Type): estructura de datos que se fusiona sola sin conflictos (ej.: Google Docs).

### En el chat
- El servidor recibe mensajes de varios clientes: ¿en qué orden los procesa? Con Lamport → cola de prioridad por timestamp → **todos ven el mismo orden** y el orden es reproducible para depurar.
- **Lab 5**: agregar `int lamport_timestamp` al struct del mensaje, reloj en cliente y servidor, log `[L=%d]`.

---

## Semana 6 — Modelos de consistencia

### ¿Qué es consistencia?
Garantía de que **todas las réplicas convergen al mismo valor** y de que las lecturas reflejan las escrituras de forma predecible. "Si escribo X=5, ¿qué veo al leer X?"
Pregunta de la clase: ¿a qué pilar de la seguridad se asocia? → **Integridad** (tríada CIA).

Por qué es difícil: réplicas múltiples, escrituras concurrentes, latencia variable, fallos de nodos, **particiones de red**.

**Ejemplo del banco**: saldo $100. +$50 en la réplica A y −$30 en la réplica B, sin replicar todavía → A=$150, B=$70, C=$100. Lo correcto es $120, y **ninguna réplica lo tiene**.

### Los 3 modelos
| Modelo | Garantía | Implementación | Pros / contras | Uso |
|---|---|---|---|---|
| **Fuerte** (linearizability) | Una lectura siempre ve la **última** escritura | **2PC** (replicación síncrona) | ✓ fácil de razonar, sin conflictos · ✗ lento, se bloquea si cae una réplica | banca, inventario |
| **Eventual** | Si **no hay más escrituras**, eventualmente todos convergen (no dice cuándo) | replicación **asíncrona**: escribe local, responde y propaga después | ✓ rápido, disponible, escalable · ✗ lecturas viejas, conflictos | DNS, redes sociales, carritos |
| **Causal** | Escrituras **causalmente relacionadas** se ven en el mismo orden en todas partes; las concurrentes pueden verse en distinto orden | **Vector clocks**: aplicar un write solo cuando ya se aplicaron sus causas | ✓ equilibrio, intuitivo · ✗ overhead, complejidad media | chats, colaboración |

**Ejemplo causal**: Alice pregunta "¿Vamos al cine?" y Bob responde "Sí, vamos". Carol **siempre** debe ver primero a Alice. No importa el retraso, importa el orden.

### Two-Phase Commit (2PC)
1. **PREPARE**: el coordinador pregunta a todos "¿pueden commitear X=10?" → cada uno responde Sí o No.
2. **COMMIT / ABORT**: si **todos** dijeron Sí → COMMIT; si no → ABORT.

Es **atómico** (todo o nada). Problema: espera a **todas** las réplicas; si una no responde, todo se detiene.

### Resolver conflictos (eventual / multi-master)
1. **Last-Write-Wins (LWW)**: gana el timestamp mayor. Es simple, pero **pierde datos**.
2. **Vector clocks + merge de la aplicación**: detectan el conflicto y la app decide (ej.: Amazon muestra todos los ítems del carrito y el usuario resuelve).
3. **CRDTs**: merge automático (ej.: G-Counter → `max(10, 15) = 15`).

### Teorema CAP (Brewer, 2000)
Solo se pueden garantizar **2 de 3**:
- **C** (Consistency): toda lectura ve la escritura más reciente.
- **A** (Availability): toda petición recibe respuesta.
- **P** (Partition tolerance): el sistema sigue funcionando aunque se pierdan mensajes.

Como en la práctica **las particiones ocurren**, la elección real es:
- **CP**: ante una partición, el lado **minoritario rechaza** escrituras → consistente pero menos disponible. *HBase, MongoDB, Spanner.*
- **AP**: **ambos lados aceptan** escrituras y después se reconcilian → disponible pero con datos viejos y conflictos. *Cassandra, DynamoDB, Riak.*

**PACELC** (Abadi, 2012): *si hay **P**artición → elegir **A** o **C**; si no (**E**lse) → elegir **L**atencia o **C**onsistencia.*

### En el chat
Recomendación del profe: **consistencia fuerte**. Con un servidor único como coordinador, este serializa todos los mensajes, así que la consistencia fuerte "sale gratis". Las notificaciones de sistema pueden ser eventuales.

### Lab K-V (3 réplicas, 3 modos)
- **Fuerte**: escribir en las 3 y OK solo si responden las 3.
- **Eventual**: escribir en 1, responder y propagar en un thread.
- **Quórum W=2, R=2**: OK con 2 ACK; leer de 2 y quedarse con la **versión más alta**. Funciona porque **W + R > N** (2 + 2 > 3), así que siempre se cruza con al menos una réplica actualizada.
- Experimentos: latencia, **qué pasa si muere 1 réplica** (fuerte falla, eventual y quórum siguen), *read-your-writes*.

---

## Semana 7 — Replicación y tolerancia a fallos ⭐ (la clase más reciente)

### ¿Por qué replicar?
1. **Disponibilidad**: si un nodo cae, otro sigue atendiendo.
2. **Rendimiento**: el cliente usa la réplica más cercana (200 ms → 20 ms).
3. **Escalabilidad**: más carga → más nodos.

### Primary-Backup (Master-Slave) — "el director"
- **Solo el primary acepta escrituras**; las lecturas van al primary o a cualquier backup.
- El primary envía un **log de replicación** a los backups.
- ✓ Simple, sin conflictos de escritura, consistencia fácil.
- ✗ El primary es **cuello de botella** y, si cae, hay **downtime** hasta elegir otro.
- *MySQL, PostgreSQL, Redis.*

**Log-based replication**: el primary aplica la operación, la anota en el log (`seq, op, T`) y la envía. El backup **solo aplica si `seq == last_applied + 1`**; si no, pide las entradas que le faltan.

**Failover**: el primary cae → los backups lo detectan por **timeout de heartbeat** → se elige un nuevo primary según:
1. el **más actualizado** (log más completo),
2. la mayor prioridad configurada,
3. el **ID más bajo** como desempate.

Downtime típico: 5–30 s.

### Multi-Master
- **Cualquier nodo acepta escrituras**; la replicación es bidireccional.
- ✓ Sin cuello de botella, alta disponibilidad, bueno para distribución geográfica.
- ✗ **Conflictos** (se resuelven con LWW, vector clocks o CRDTs), más complejo.
- *Cassandra, CouchDB, Riak.*

| Quiero favorecer… | Diseño |
|---|---|
| Simplicidad / consistencia | Primary-Backup |
| Resiliencia | Primary + backups + heartbeat + failover |
| Velocidad de escritura / disponibilidad | Multi-master (con eventual consistency) |
| Datos al día | Replicación síncrona (fuerte) |

### Detección de fallos
**Problema fundamental**: no se puede distinguir un nodo **caído**, un nodo **lento**, una **red particionada** o un **mensaje perdido**. Solo queda suponer usando **timeouts**.

**Heartbeats**: mensajes periódicos de "estoy vivo" (ej.: cada 1 s). Si no llega ninguno en T segundos (ej.: 3), se marca el nodo como DOWN. Si vuelve a llegar uno → "back UP".
```c
typedef struct { int node_id; time_t last_heartbeat; bool is_alive; } NodeStatus;
// al recibir:   nodes[id].last_heartbeat = time(NULL);
// monitor (1 s): if (now - nodes[i].last_heartbeat > 3) → DOWN
```

**El dilema del timeout**
- **Corto**: detecta rápido, pero da **falsos positivos** (un nodo lento parece muerto).
- **Largo**: pocos falsos positivos, pero detecta lento.
- **Adaptativo**: promedio móvil exponencial; `timeout = media + 4·desviación`.

### Recuperación ante fallos
1. **Checkpointing**: guardar el estado cada cierto tiempo; al recuperar, cargar el último checkpoint y repetir el log desde ahí.
2. **Log replay**: repetir **todo** el log desde el inicio. Es completo pero lento → combinarlo con checkpoints.
3. **State transfer**: copiar el estado completo de otra réplica. Es rápido, pero necesita una réplica viva y actualizada.

(Esto conecta con el Ej 13 **snapshot de Chandy-Lamport**: un checkpoint *consistente* de un sistema distribuido.)

### Split-brain
Con una partición, el primary piensa "murió el backup" y el backup piensa "murió el primary" y se promueve → **quedan 2 primaries**.

**Solución: quórum.** Para decidir hace falta **mayoría**: `QUORUM = N/2 + 1`. Con 5 nodos, el lado con 3 opera y el lado con 2 se bloquea. **Regla de oro: número impar de nodos (3, 5, 7).**

### Caso real: Redis Sentinel
Los sentinels hacen PING al primary cada 1 s:
- Si **1** sentinel no recibe PONG → **SDOWN** (subjetivo).
- Si una **mayoría** está de acuerdo → **ODOWN** (objetivo).
- Luego **votan** y promueven al backup más actualizado.

### Lab Health Checking (sobre el chat)
- **Servidor**: `ClientStatus {client_id, last_heartbeat, is_active}` + un monitor que cada 1 s revisa los timeouts.
- **Cliente**: envía `HEARTBEAT` cada 1 s.
- **Timeout**: opción A, expulsar de inmediato; opción B, **grace period** (marcar inactivo, y expulsar si pasan más de N s).
- **Tests**: Ctrl+C → detectado en ~3 s; Ctrl+Z + fg rápido → vuelve a estar activo; Ctrl+Z más de 35 s → expulsado.

---

## Semana 8 — Consenso y Raft (adelanto, probablemente no entra)
- **Consenso**: varios nodos acuerdan **un** valor. Propiedades: Termination, Agreement, Validity, Integrity.
- **FLP**: en un sistema asíncrono con 1 proceso que puede fallar, el consenso determinístico es imposible. Raft lo esquiva **asumiendo timeouts**.
- **Raft**: estados Follower → (timeout) Candidate → (mayoría de votos) Leader. Tiene **terms** (máximo 1 líder por term) y **timeouts de elección aleatorios** (150–300 ms) para evitar el *split vote*.
- **RequestVote**: se vota 1 vez por term y solo a un candidato con log al menos tan actualizado como el propio.
- **AppendEntries**: sirve de heartbeat y de replicación. Una entrada se hace commit con ACK de la **mayoría**. El consistency check usa `prev_log_index/term`.
- **Safety**: Election safety, Leader append-only, Log matching, Leader completeness, State machine safety.
