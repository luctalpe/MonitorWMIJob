# MonitorWMIJob
Tool to monitor WMI Job (controlling WMIPRVSE processes )

MonitorWmiJob.exe -?

Usage : MonitorWMIJob [-DISPLAY] [-IDNA] [-MONITOR]

        -DISPLAY :
                print the memory quotas and stats for WMI Job Global\WmiProviderSubSystemHostJob
        -MONITOR :
                will monitor the WMI job
                and will execute script.cmd, the first time a job memory quota violation will occur
        -IDNA    :
                will suspend quota limits to allow IDNA (TTTRacer.exe) to capture traces
                Quotas limitation will be restored at exit time

Warning: to access to this WMI Job, you need to run this program in a system context (you can use psexec -i -s cmd.exe from sysinternals)
