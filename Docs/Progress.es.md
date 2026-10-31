# Minkar — El avance de un vistazo

El estado del roadmap en diagramas; [ROADMAP.es.md](ROADMAP.es.md) lo describe y
[ROADMAP.md](ROADMAP.md) (en inglés) es la fuente de verdad. Cada cambio allí se
refleja aquí. En inglés: [Progress.md](Progress.md). Actualizado el 31 oct 2026.

Colores: verde hecho, amarillo parcial o en curso, gris sin empezar.

## Ruta crítica y gates

```mermaid
flowchart LR
    P0["Fase 0<br/>Build, toolchain, CI<br/><b>M0 cumplido</b>"]:::done
    P1["Fase 1<br/>Arranque Darwin<br/><b>M1: falta el PC 2</b>"]:::partial
    P2["Fase 2<br/>Userland base<br/><b>M2 cumplido, cerrada</b>"]:::done
    P3["Fase 3<br/>Frameworks y Swift<br/><b>M3</b>"]:::partial
    P4["Fase 4<br/>Gráficos, WindowServer<br/><b>M4</b>"]:::partial
    P5["Fase 5<br/>Escritorio Gienah<br/><b>M5</b>"]:::todo
    P6["Fase 6<br/>Apps y distribución<br/><b>M6 = Minkar 19.6.0</b>"]:::todo
    P7["Fase 7<br/>Compatibilidad binaria, arm64<br/><b>M7</b>"]:::todo
    PERF["Minkar 21.0<br/>API 21 + rendimiento"]:::todo
    GPU["Frente de GPU<br/>G1–G5"]:::todo

    P0 --> P1 --> P2 --> P3 --> P4 --> P5 --> P6
    P6 -.-> P7
    P6 -.-> PERF
    GPU -.-> PERF
    P4 -.-> GPU

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Fase 3: libSystem y Foundation

```mermaid
flowchart TB
    subgraph LS["libSystem"]
        L1["Imports de las apps de referencia<br/>308 de 308"]:::done
        L2["os_log to syslogd<br/>os_activity"]:::done
        L3["@available<br/>_availability_version_check"]:::done
        L4["launch_activate_socket"]:::done
        L8["sqlite3, libxar,<br/>libcompression"]:::done
        L5["Stubs ruidosos<br/>mecanismo + 887 sitios"]:::partial
        L6["logd"]:::todo
        L7["Shared cache de dyld"]:::todo
    end
    subgraph FW["Frameworks"]
        F1["Auditoría de Foundation<br/>NSProcessInfo, NSOperationQueue,<br/>cola principal en el run loop"]:::partial
        F2["NSURLSession sobre libcurl<br/>quedan cookies, caché, métricas"]:::partial
        F3["CF-Lite hasta la API 21<br/>puente de arrays y diccionarios hecho"]:::partial
        F4["Security, LaunchServices"]:::todo
    end
    subgraph SW["Swift 6"]
        S1["Prototipo 1: swiftc"]:::todo
        S2["Prototipo 2: libswiftCore"]:::todo
        S3["Overlays, overlay de Foundation"]:::todo
    end
    S1 --> S2 --> S3
    F4 -.-> F2

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Pila gráfica: componentes por estado

Cada caja indica el paso de la secuencia (abajo) que la cierra.

```mermaid
flowchart TB
    subgraph APPS["Apps"]
        AK["AppKit (Cocotron)<br/>paso 7"]:::partial
    end
    subgraph TEXT["Texto e imágenes"]
        CT["Core Text on<br/>HarfBuzz/FreeType<br/>paso 6"]:::todo
        IMG["giflib, libwebp<br/>paso 6"]:::todo
        CS["ColorSync on<br/>Little CMS<br/>paso 6"]:::todo
    end
    subgraph DRAW["Dibujo y composición"]
        CG["Core Graphics<br/>con FreeType"]:::done
        LIBS["pixman, libpng,<br/>libjpeg, libtiff"]:::done
        QC["QuartzCore, Onyx2D"]:::partial
        CA["Compositor de Core Animation<br/>(primero GPU)<br/>paso 5"]:::todo
        IOS["IOSurface<br/>paso 4"]:::todo
    end
    subgraph SERVER["Servidor de ventanas"]
        WS["WindowServer<br/>compila, nunca se ejecutó<br/>paso 2"]:::partial
    end
    subgraph INPUT["Entrada"]
        HIDH["IOKit/hidsystem<br/>headers"]:::done
        USB["xHCI + HID<br/>(MinkarUSB)"]:::done
        KBD["PS/2 keyboard<br/>llega a la consola<br/>paso 1"]:::partial
        MOUSE["ApplePS2Mouse<br/>paso 1"]:::todo
        HIDS["Entrega de eventos<br/>de IOHIDSystem<br/>paso 1"]:::todo
    end
    subgraph DISPLAY["Pantalla"]
        GOP["Consola GOP<br/>(IOGOPFramebuffer)"]:::done
        VGPU["virtio-gpu 2D<br/>paso 3"]:::todo
        GPUD["Driver de GPU, Mesa,<br/>Metal (G1–G5)"]:::todo
    end

    AK --> CT & QC
    CT --> CG
    QC --> CA --> IOS
    CA --> WS
    WS --> GOP & VGPU & GPUD
    WS --> HIDS
    HIDS --> KBD & MOUSE & USB
    CG --> LIBS
    IMG --> CG
    CS --> CG

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#fff
    classDef partial fill:#f9a825,stroke:#f57f17,color:#000
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

## Pila gráfica: secuencia de desarrollo

```mermaid
flowchart LR
    S1["1. Eventos de IOHIDSystem<br/>y ratón PS/2"]:::next
    S2["2. WindowServer<br/>sobre el framebuffer GOP"]:::todo
    S3["3. virtio-gpu 2D<br/>(FreeBSD, BSD-2)"]:::todo
    S4["4. IOSurface<br/>(memoria compartida Mach)"]:::todo
    S5["5. Compositor de Core Animation<br/>(Vulkan; pixman para arrancar)"]:::todo
    S6["6. HarfBuzz/FreeType, giflib,<br/>libwebp, Little CMS"]:::todo
    S7["7. AppKit moderno,<br/>vistas con capas"]:::todo
    S1 --> S2 --> S3 --> S4 --> S5 --> S6 --> S7

    G1["G1 familia de GPU<br/>y prototipo KMS"]:::todo
    G2["G2–G5 driver del kernel, Mesa<br/>(lavapipe/llvmpipe en VMs),<br/>Metal (darling-metal, Indium)"]:::todo
    G1 --> G2
    S4 -.-> G2
    S5 -.-> G2

    classDef next fill:#1565c0,stroke:#0d47a1,color:#fff
    classDef todo fill:#9e9e9e,stroke:#616161,color:#fff
```

El paso 1 es el siguiente (azul): sin entrada, WindowServer no puede cerrar M4. G1
empieza en cuanto se decida la familia de GPU; las flechas discontinuas marcan lo
que el frente de GPU reutiliza de los pasos 4 y 5.
