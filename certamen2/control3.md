# CONTROL 03 — TALLER DE CIENCIA DE REDES
## Protocolo CENTINELA

**Tiempo:** 60 min.  
**Puntaje total:** 60 puntos

---

## Briefing de misión

Año 2247. La Fundación para la Preservación del Conocimiento Humano mantiene una red de Bóvedas en los planetas del borde exterior, donde se resguardan los últimos repositorios de ciencia y cultura de la civilización. Cada Bóveda está protegida por un enjambre de drones autónomos clase CENTINELA, diseñados para operar sin intervención humana durante décadas.

Usted es ingeniero de sistemas de la Fundación, asignado a la Bóveda Sigma-7 en el planeta Términus. Su enjambre CENTINELA está compuesto por 6 drones (C1–C6) que patrullan el perímetro de la Bóveda en turnos continuos. Cada dron está equipado con una cámara multiespectral y sensores de proximidad. El enjambre opera bajo un modelo de líder único: el dron líder coordina la formación de patrulla, asigna sectores de vigilancia y consolida los flujos de video hacia el terminal central de la Bóveda.

Los drones se comunican mediante radio mesh de corto alcance. El líder emite un heartbeat cada 5 segundos. El protocolo CENTINELA establece que si un dron no recibe heartbeat durante **3 ciclos consecutivos**, debe declarar al líder como inoperativo e iniciar un proceso de elección de nuevo líder.

La aptitud de cada dron para asumir el liderazgo se evalúa según tres criterios operacionales:

- **Batería disponible (%)**
- **Distancia al perímetro de la Bóveda (km)**
- **Índice de visibilidad (0–100)**

---

## Registro de incidente — Ciclo 1847, guardia nocturna

El dron C3, líder del enjambre, patrullaba el sector norte del perímetro. A las 03:12:00 TST (Tiempo Estándar de Términus), C3 dejó de responder. El log del dron C1 registra:

```
[03:12:05 TST] HB_REQ → C3   TIMEOUT (sin respuesta tras 2000 ms)
[03:12:12 TST] HB_REQ → C3   TIMEOUT (sin respuesta tras 2000 ms)
[03:12:19 TST] HB_REQ → C3   TIMEOUT (sin respuesta tras 2000 ms)
[03:12:19 TST] ══ PROTOCOLO CENTINELA: C3 INOPERATIVO ══
[03:12:19 TST] ══ INICIAR ELECCIÓN DE NUEVO LÍDER ══
```

### Telemetría del enjambre al momento del incidente

| Dron | Batería (%) | Dist. perímetro (km) | Visibilidad (0–100) | Estado      |
|------|-------------|----------------------|---------------------|-------------|
| C1   | 78          | 1.2                  | 85                  | Operativo   |
| C2   | 45          | 0.8                  | 92                  | Operativo   |
| C3   | —           | —                    | —                   | Inoperativo |
| C4   | 91          | 2.1                  | 70                  | Operativo   |
| C5   | 62          | 0.5                  | 88                  | Operativo   |
| C6   | 83          | 1.5                  | 45                  | Operativo   |

*Todos los drones operativos mantienen conectividad mesh entre sí. Cada dron conoce la composición completa del enjambre y puede enviar mensajes directos a cualquier otro dron operativo.*

---

## Su misión

La Bóveda Sigma-7 no puede quedar sin vigilancia coordinada. Diseñe e implemente el protocolo de elección que permita al enjambre CENTINELA recuperar su cadena de mando de forma autónoma.

### 1. Función de aptitud (20 puntos)

Defina una función de aptitud `F(batería, distancia, visibilidad)` que asigne un valor numérico a cada dron, representando su idoneidad para asumir el liderazgo del enjambre.

- **a)** Presente la fórmula de su función de aptitud y justifique el peso o tratamiento que asigna a cada criterio en el contexto de vigilancia con cámara multiespectral.
- **b)** Calcule el valor de aptitud para cada dron operativo (C1, C2, C4, C5, C6) y ordénelos de mayor a menor aptitud.

### 2. Protocolo de elección (40 puntos)

Seleccione un algoritmo de consenso o elección de líder estudiado en el curso e impleméntelo para este escenario. Debe indicar explícitamente cuál algoritmo selecciona.

- **a)** Implemente el protocolo de elección en C. El código debe modelar el intercambio de mensajes entre los drones, desde que C1 detecta la caída de C3 hasta que el nuevo líder es reconocido por todo el enjambre. **No se acepta pseudocódigo.**
- **b)** Determine qué dron resulta elegido como nuevo líder según su implementación y justifique el resultado.

---

> «La violencia es el último recurso del incompetente.»  
> — Salvor Hardin, Alcalde de Términus