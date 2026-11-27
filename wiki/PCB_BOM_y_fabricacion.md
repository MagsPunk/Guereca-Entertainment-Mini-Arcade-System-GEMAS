# PCB, BOM y fabricación

## Revisión de la placa

El paquete de fabricación archivado identifica la revisión **`PCB_GEMAS_V2(RevisaEsta)`**. Incluye dos capas de cobre, **L1 superior** y **L2 inferior**, y un contorno de **100 × 72 mm** según el Gerber `Profile`. Estas dimensiones corresponden al archivo exportado. La revisión enviada a fabricar queda pendiente de confirmar.

## Archivos de diseño y fabricación

| Archivo | Contenido |
|---|---|
| Proyecto `.PrjPcb` y hojas `.SchDoc` | Proyecto editable de Altium Designer y esquemáticos por subsistema. |
| `PCB_GEMAS_V2(RevisaEsta).PcbDoc` | Diseño editable de la PCB V2. |
| Archivo `.BomDoc` | Lista de materiales editable en Altium. Es la fuente para consultar designadores, cantidades y números de parte. |
| Archivo `.OutJob` | Configuración de las salidas del proyecto. |
| [`GERBER_GEMAS.zip`](../hardware/fabricacion/GERBER_GEMAS.zip) | Paquete Gerber X2 de V2. Incluye cobre, máscara antisoldante, serigrafía, pasta, contorno y taladros metalizados y no metalizados. |
| Reportes `.drc` y `.html` | Resultados de la comprobación de reglas para cada revisión archivada. |

Las capas `Paste_Top` y `Paste_Bot` corresponden a las aperturas del esténcil. El archivo `Profile` define el contorno de la placa. Los archivos `PTH_Drill` y `NPTH_Drill` identifican los taladros metalizados y no metalizados, respectivamente.

## Verificación previa a fabricación

El reporte DRC del **6 de noviembre de 2026** registra **0 advertencias y 0 infracciones** para V2. El archivo editable `PcbDoc` tiene una fecha de modificación posterior al reporte. Para una nueva orden de fabricación, se debe ejecutar el DRC sobre la revisión que se vaya a enviar, regenerar los Gerbers y revisar el contorno y los taladros en el visor CAM.

---

[Inicio](Home.md) · [Anterior · Pinout e interfaces](Pinout_e_interfaces.md) · [Siguiente · Carcasa y ensamblaje](Carcasa_y_ensamblaje.md)
