package com.example.comfythirdperson;

import com.mojang.blaze3d.platform.InputConstants;
import net.minecraft.client.Camera;
import net.minecraft.client.CameraType;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.network.chat.Component;
import net.minecraft.util.Mth;
import net.neoforged.api.distmarker.Dist;
import net.neoforged.bus.api.IEventBus;
import net.neoforged.fml.ModContainer;
import net.neoforged.fml.common.Mod;
import net.neoforged.fml.config.ModConfig;
import net.neoforged.neoforge.client.event.ClientTickEvent;
import net.neoforged.neoforge.client.event.InputEvent;
import net.neoforged.neoforge.client.event.RegisterKeyMappingsEvent;
import net.neoforged.neoforge.client.event.ViewportEvent;
import net.neoforged.neoforge.common.NeoForge;

import java.util.Locale;

@Mod(value = ComfyThirdPerson.MODID, dist = Dist.CLIENT)
public class ComfyThirdPerson {
    public static final String MODID = "comfythirdperson";

    private static final double MIN_DISTANCE = -3.0;
    private static final double MAX_DISTANCE = 20.0;
    private static final double DEFAULT_SHOULDER = 0.75;

    private static final KeyMapping SWAP_SHOULDER = new KeyMapping(
            "key.comfythirdperson.swap_shoulder", InputConstants.KEY_V, "key.categories.comfythirdperson");
    private static final KeyMapping RESET_CAMERA = new KeyMapping(
            "key.comfythirdperson.reset_camera", InputConstants.KEY_B, "key.categories.comfythirdperson");

    public ComfyThirdPerson(IEventBus modBus, ModContainer container) {
        container.registerConfig(ModConfig.Type.CLIENT, Config.SPEC);
        modBus.addListener(this::registerKeys);
        NeoForge.EVENT_BUS.addListener(this::onClientTick);
        NeoForge.EVENT_BUS.addListener(this::onScroll);
        NeoForge.EVENT_BUS.addListener(this::onCameraAngles);
    }

    private void registerKeys(RegisterKeyMappingsEvent event) {
        event.register(SWAP_SHOULDER);
        event.register(RESET_CAMERA);
    }

    private void onClientTick(ClientTickEvent.Post event) {
        Minecraft mc = Minecraft.getInstance();
        if (mc.player == null) return;

        // Пропуск вида "спереди" при нажатии F5
        if (Config.SKIP_FRONT_VIEW.get() && mc.options.getCameraType() == CameraType.THIRD_PERSON_FRONT) {
            mc.options.setCameraType(CameraType.FIRST_PERSON);
        }

        while (SWAP_SHOULDER.consumeClick()) {
            double current = Config.SHOULDER_OFFSET.get();
            double next = current == 0.0 ? DEFAULT_SHOULDER : -current;
            Config.SHOULDER_OFFSET.set(next);
            Config.SPEC.save();
            message(mc, Component.translatable("msg.comfythirdperson.shoulder", fmt(next)));
        }

        while (RESET_CAMERA.consumeClick()) {
            Config.DISTANCE_OFFSET.set(0.0);
            Config.SHOULDER_OFFSET.set(0.0);
            Config.SPEC.save();
            message(mc, Component.translatable("msg.comfythirdperson.reset"));
        }
    }

    // Alt + колесо мыши = приблизить/отдалить камеру
    private void onScroll(InputEvent.MouseScrollingEvent event) {
        Minecraft mc = Minecraft.getInstance();
        if (mc.player == null || mc.screen != null) return;
        if (mc.options.getCameraType().isFirstPerson()) return;
        if (!Screen.hasAltDown()) return;

        double delta = event.getScrollDeltaY();
        if (delta == 0) return;

        double next = Mth.clamp(
                Config.DISTANCE_OFFSET.get() - Math.signum(delta) * Config.ZOOM_STEP.get(),
                MIN_DISTANCE, MAX_DISTANCE);
        Config.DISTANCE_OFFSET.set(next);
        Config.SPEC.save();
        event.setCanceled(true);
        message(mc, Component.translatable("msg.comfythirdperson.distance", fmt(4.0 + next)));
    }

    // Сдвиг камеры после того, как ваниль её расставила
    private void onCameraAngles(ViewportEvent.ComputeCameraAngles event) {
        Camera camera = event.getCamera();
        if (!camera.isDetached()) return;

        float distance = Config.DISTANCE_OFFSET.get().floatValue();
        float side = Config.SHOULDER_OFFSET.get().floatValue();
        if (distance == 0f && side == 0f) return;

        // move(вперёд, вверх, влево): минус по первому = дальше от игрока, минус по третьему = вправо
        camera.move(-distance, 0f, -side);
    }

    private static void message(Minecraft mc, Component component) {
        if (mc.player != null) mc.player.displayClientMessage(component, true);
    }

    private static String fmt(double value) {
        return String.format(Locale.ROOT, "%.1f", value);
    }
}
