#
# Campus Agent - Contest 2026 Team 307
#

if(CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT)

  nuttx_add_application(
    NAME
    ${CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_PROGNAME}
    PRIORITY
    ${CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_PRIORITY}
    STACKSIZE
    ${CONFIG_LVX_CONTEST2026_307_CAMPUS_AGENT_STACKSIZE}
    SRCS
    main.c
    course_manager.c
    reminder_engine.c
    sedentary_monitor.c
    ui_briefing.c
    vibrate.c
    button_handler.c
    dialogue.c
  )

endif()
