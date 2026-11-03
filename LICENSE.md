# Licencias

Este repositorio usa una licencia distinta para cada tipo de contenido.

| Contenido | Licencia | Identificador SPDX | Texto oficial |
| --- | --- | --- | --- |
| Hardware: esquemáticos, PCB, librerías propias, Gerbers (`hardware/`) | CERN Open Hardware Licence v2 — Strongly Reciprocal | `CERN-OHL-S-2.0` | https://ohwr.org/cern_ohl_s_v2.txt |
| Documentación (`docs/`, archivos `.md`, imágenes) | Creative Commons Atribución-CompartirIgual 4.0 | `CC-BY-SA-4.0` | https://creativecommons.org/licenses/by-sa/4.0/legalcode.es |
| Firmware (`firmware/`) | MIT | `MIT` | https://opensource.org/license/mit |

## Qué significa en la práctica

- **Puedes** estudiar, fabricar, modificar y vender el hardware y usar la documentación.
- **Si distribuyes una versión modificada del hardware o de la documentación**, debes publicarla bajo la misma licencia y dar crédito al proyecto original.
- **El firmware** se puede usar en cualquier proyecto, incluso cerrado, conservando el aviso de copyright.

## Sin garantía

El hardware, la documentación y el firmware se entregan **"tal cual", sin garantía de ningún tipo**. Este equipo maneja voltajes y corrientes peligrosos; el autor no se hace responsable de daños, lesiones o pérdidas derivados de su uso.

## Componentes de terceros

- Las **hojas de datos** de los fabricantes (Toshiba, onsemi, Infineon, Texas Instruments, etc.) no se incluyen: se citan por nombre y fuente.
- Los **modelos 3D** y footprints de la librería oficial de KiCad conservan su propia licencia (CC BY-SA 4.0 con excepción para diseños que los usan).

---

Copyright (c) 2026 J.G — Proyecto Isla del Sol

> Nota: antes de publicar el hardware, copia el texto completo de la CERN-OHL-S v2 (enlace arriba) en `hardware/LICENSE`, y el texto MIT en `firmware/LICENSE`.
