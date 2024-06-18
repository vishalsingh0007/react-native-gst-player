package com.gstreamertest.utils.manager;

import java.util.HashMap;
import java.util.Map;

/**
 * Created by vishal singh on 16/o6/2024.
 */

public enum Command {

    // callable methods from JS
    setState, recreateView;

    // Index for js association
    private int index;

    public final int getIndex() {
        return index;
    }

    // Command mapping for js calls (name, index)
    private static HashMap<String, Integer> commandMap = new HashMap();

    public static Map<String, Integer> getCommandsMap() {
        return commandMap;
    }

    // Static method for easy command calling
    public static boolean is(int commandType, Command command) {
        return Command.values()[commandType].getIndex() == command.getIndex();
    }

    // Preparing commands in an automated way
    static {
        for (int i = 0; i < Command.values().length; i++) {
            Command command = Command.values()[i];
            command.index = i;

            commandMap.put(command.name(), i);
        }
    }
}
