#
# Campus Agent - Contest 2026 Team 307
#

include $(APPDIR)/Make.defs

ifeq ($(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT), y)

PROGNAME  = $(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_PROGNAME)
PRIORITY  = $(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_PRIORITY)
STACKSIZE = $(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_STACKSIZE)
MODULE    = $(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT)

CSRCS   = course_manager.c reminder_engine.c sedentary_monitor.c \
          ui_briefing.c vibrate.c button_handler.c dialogue.c
MAINSRC = main.c

# SiFli HAL 头文件路径
CFLAGS += -I$(APPDIR)/../vendor/sifli/chips/drivers/Include
CFLAGS += -I$(APPDIR)/../vendor/sifli/chips/sf32lb52
CFLAGS += -I$(APPDIR)/../vendor/sifli/boards/sf32lb52/lckfb_huangshan_pi/include

endif

include $(APPDIR)/Application.mk
