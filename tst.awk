/ONBATT/ {
    STATE=$3
    printf("On Battery %s\n", STATE);
    if (STATE == "true" ) {
        print "Shutdown now"
    } else {
        print "All is well."
    }
}
/CYCLECOUNT/ {
    COUNT=$3
    system("date")
    printf("%s times around\n", COUNT);
}
