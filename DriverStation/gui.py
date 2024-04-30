import imgui
from enum import Enum

CONTROL_BAR_HEIGHT = 300
TELEMETRY_PANEL_WIDTH = 400
SHOW_IMGUI_TEST_WINDOW = False

CONFIRM_POPOP_WIDTH = 300
CONFIRM_POPUP_HEIGHT = 120
POPUP_IMGUI_FLAGS=imgui.WINDOW_NO_RESIZE | imgui.WINDOW_NO_MOVE
LED_INDICATOR_IMGUI_FLAGS=imgui.COLOR_EDIT_NO_BORDER | imgui.COLOR_EDIT_NO_TOOLTIP | imgui.COLOR_EDIT_NO_INPUTS

class GuiAction:
    def __init__(self, text, callback):
        self.text = text
        self.callback = callback

    def execute(self):
        self.callback()

class Gui: 
    def __init__(self):
        self.current_action = None
        pass
    
    def led_indicator(self, text, enabled):
        r = 0
        g = 0
        b = 0

        if enabled:
            g = 1
        else:
            r = 1

        imgui.text(text)
        imgui.same_line()
        imgui.color_button(f"{text}-LED", r, g, b, 1, LED_INDICATOR_IMGUI_FLAGS, 50)

    def button_with_popup(self, text, action):
        if(imgui.button(text)):
            self.current_action = GuiAction(text, action)

    def draw_ctrl_bar(self, state):
        with imgui.begin_group():
            imgui.begin_child(
                "STATUS_LEDS", border=False,
                flags=imgui.WINDOW_NO_SCROLLBAR,
                height=90,
                width=300
            )
            with imgui.begin_group():
                self.led_indicator(" COMMS", False)
                self.led_indicator("RP2040", False)

            imgui.same_line(spacing=50)

            with imgui.begin_group():
                self.led_indicator("JOYSTICK", False)
                self.led_indicator(" ENABLED", state.is_robot_enabled())
            imgui.end_child()

            with imgui.begin_group():
                imgui.text("ROBOT STATE")

                if state.is_robot_enabled():
                    if imgui.button("Disable Robot"):
                        state.disable_robot()
                else:
                    if imgui.button("Enable Robot"):
                        state.enable_robot()

                self.button_with_popup("Run Auto Dig", state.run_auto_dig)
                self.button_with_popup("Restart Code", state.restart_robot_code)
                self.button_with_popup("Reboot Pi", state.reboot_pi)
                self.button_with_popup("Reboot RP2040", state.reboot_rp2040)

            imgui.same_line(spacing=50)

            with imgui.begin_group():
                imgui.text(f"LINKAGE: {state.get_linkage_state()}")
                self.button_with_popup("Deploy Linkage", state.deploy_intake)
                self.button_with_popup("Set Manual Control", state.manual_control_intake)
                if imgui.button("Reset Encoder"):
                    self.current_action = GuiAction('Reset Encoder', state.reset_intake_encoder)

        imgui.same_line(spacing=50)

        with imgui.begin_group():
            imgui.text("CONSOLE")
            imgui.same_line(spacing=10)
            imgui.button("Clear")

            imgui.begin_child(
                "Child 2", border=True,
                flags=imgui.WINDOW_NO_SCROLLBAR
            )
            imgui.text_wrapped("This text will wrap around.\n[INFO] this is a log message")
            imgui.end_child()
        
    def draw_telemetry(self):
        imgui.text("Intake height: XXX")
        imgui.text("Intake speed: XXX")
        imgui.text("Intake torque: XXX")
        imgui.text("")
        imgui.text("Dump speed: XXX")
        imgui.text("Dump torque: XXX")

    def draw_confirmation_box(self, window_width, window_height):
        if self.current_action != None:
            imgui.open_popup("Action Confirmation")

            imgui.set_next_window_size(CONFIRM_POPOP_WIDTH, CONFIRM_POPUP_HEIGHT)
            imgui.set_next_window_position((window_width / 2) - (CONFIRM_POPOP_WIDTH  / 2), (window_height / 2) - (CONFIRM_POPUP_HEIGHT / 2))
            with imgui.begin_popup_modal("Action Confirmation", flags=POPUP_IMGUI_FLAGS) as select_popup:
                if select_popup.opened:
                    imgui.text("Are you sure:")
                    imgui.text("")
                    if imgui.button(self.current_action.text):
                        self.current_action.execute()
                        self.current_action = None
                    imgui.same_line()
                    if imgui.button("Cancel"):
                        self.current_action = None
                


    def render(self, state, window_size):
        window_width, window_height = window_size

        imgui.set_next_window_size(window_width, CONTROL_BAR_HEIGHT)
        imgui.set_next_window_position(0, window_height - CONTROL_BAR_HEIGHT)
        imgui.begin("Control Bar", flags=imgui.WINDOW_NO_COLLAPSE | imgui.WINDOW_NO_RESIZE | imgui.WINDOW_NO_MOVE)
        self.draw_ctrl_bar(state)
        imgui.end()

        imgui.set_next_window_size(TELEMETRY_PANEL_WIDTH, window_height - CONTROL_BAR_HEIGHT)
        imgui.set_next_window_position(0, 0)
        imgui.begin("Telemetry", flags=imgui.WINDOW_NO_COLLAPSE | imgui.WINDOW_NO_RESIZE | imgui.WINDOW_NO_MOVE)
        self.draw_telemetry()
        imgui.end()

        self.draw_confirmation_box(window_width, window_height)

        if SHOW_IMGUI_TEST_WINDOW:
            imgui.show_test_window()