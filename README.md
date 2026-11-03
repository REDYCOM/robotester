# 🤖 RoboTester

Página web para que los estudiantes verifiquen las conexiones de su robot (Arduino UNO/Nano) antes de programarlo.

**Proyectos:** Resuelve laberintos · Sumo · Seguidor de línea · Futbolista

## Cómo se usa (estudiantes)
1. Subir `firmware/RoboTester/RoboTester.ino` al Arduino (una sola vez).
2. Cerrar el Monitor Serie del Arduino IDE.
3. Abrir la página en **Chrome o Edge** (computadora), elegir el proyecto y pulsar **Conectar Arduino**.
4. Seguir las pruebas. Si algo falla, la página dice qué revisar.
5. Al terminar, volver a subir el programa del robot.

Enlaces directos por proyecto: `.../#laberinto`, `.../#sumo`, `.../#seguidor`, `.../#futbolista`.
Modo demo sin Arduino: `.../?demo#sumo`. Prueba guiada paso a paso: `guiado.html`. Panel libre para el profe: `libre.html`.

## Cambiar pines o proyectos (profe)
Editar el objeto `PROJECTS` al inicio del `<script>` en `index.html`.

## Publicar
GitHub Pages: Settings → Pages → Branch `main` / root. (Web Serial requiere HTTPS, GitHub Pages ya lo tiene.)
