package com.example.comfythirdperson;

import net.neoforged.neoforge.common.ModConfigSpec;

public class Config {
    private static final ModConfigSpec.Builder BUILDER = new ModConfigSpec.Builder();

    public static final ModConfigSpec.DoubleValue DISTANCE_OFFSET = BUILDER
            .comment("Дополнительная дистанция камеры в 3-м лице (минус = ближе, плюс = дальше). Ванильная дистанция = 4 блока.")
            .defineInRange("distanceOffset", 0.0, -3.0, 20.0);

    public static final ModConfigSpec.DoubleValue SHOULDER_OFFSET = BUILDER
            .comment("Смещение камеры в сторону (плюс = через правое плечо, минус = через левое, 0 = по центру).")
            .defineInRange("shoulderOffset", 0.0, -2.0, 2.0);

    public static final ModConfigSpec.DoubleValue ZOOM_STEP = BUILDER
            .comment("Шаг изменения дистанции за один щелчок колеса (Alt + колесо).")
            .defineInRange("zoomStep", 0.5, 0.1, 3.0);

    public static final ModConfigSpec.BooleanValue SKIP_FRONT_VIEW = BUILDER
            .comment("Пропускать вид спереди при переключении F5 (первое лицо -> третье лицо сзади -> первое лицо).")
            .define("skipFrontView", true);

    public static final ModConfigSpec SPEC = BUILDER.build();
}
