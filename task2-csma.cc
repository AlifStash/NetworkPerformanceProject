// Task 2: Shared-Medium Contention Study
// 10-node CSMA LAN with variable number of UDP senders to one sink

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Task2CsmaContention");

int main(int argc, char *argv[])
{
    // ---- 1. PARAMETERS (changeable from command line) ----
    uint32_t nNodes    = 10;        // total nodes on the LAN
    uint32_t nSenders  = 1;         // change this: 1, 3, then 5
    double   simTime   = 10.0;      // simulation duration (seconds)
    uint32_t pktSize   = 1024;      // bytes per UDP packet
    std::string dataRate = "5Mbps"; // each sender's offered load

    CommandLine cmd;
    cmd.AddValue("nSenders", "Number of active UDP senders", nSenders);
    cmd.AddValue("simTime",  "Simulation time in seconds",   simTime);
    cmd.Parse(argc, argv);

    // ---- 2. CREATE NODES ----
    NodeContainer nodes;
    nodes.Create(nNodes);   // Node 0 = sink, Nodes 1..9 = potential senders

    // ---- 3. CREATE THE CSMA CHANNEL ----
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // ---- 4. INSTALL INTERNET STACK ----
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // ---- 5. SET UP THE SINK (Node 0) ----
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // ---- 6. SET UP THE SENDERS (Nodes 1..nSenders) ----
    for (uint32_t i = 1; i <= nSenders; ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
        onoff.SetAttribute("OnTime",
            StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime",
            StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue(dataRate));
        onoff.SetAttribute("PacketSize", UintegerValue(pktSize));

        ApplicationContainer app = onoff.Install(nodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime - 1.0));
    }

    // ---- 7. NETANIM ANIMATION TRACE ----
    AnimationInterface anim("task2-csma-" + std::to_string(nSenders) + ".xml");

    // Lay nodes out in a row so the animation is readable
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);   // sink on the left
    for (uint32_t i = 1; i < nNodes; ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), 10.0 * i, 20.0);
    }

    // ---- 8. FLOW MONITOR ----
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // ---- 9. RUN THE SIMULATION ----
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ---- 10. COLLECT AND PRINT STATISTICS ----
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    std::cout << "\n===== RESULTS: " << nSenders << " sender(s) =====\n";

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    double   totalThroughput = 0.0;

    for (auto &flow : monitor->GetFlowStats())
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        double durationSec = flow.second.timeLastRxPacket.GetSeconds()
                           - flow.second.timeFirstTxPacket.GetSeconds();
        double throughputKbps =
            (flow.second.rxBytes * 8.0) / (durationSec * 1000.0);
        double pdr = (flow.second.txPackets > 0)
                   ? (100.0 * flow.second.rxPackets / flow.second.txPackets)
                   : 0.0;

        std::cout << "Flow " << flow.first
                  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
                  << "  Tx Packets : " << flow.second.txPackets << "\n"
                  << "  Rx Packets : " << flow.second.rxPackets << "\n"
                  << "  Throughput : " << throughputKbps << " Kbps\n"
                  << "  PDR        : " << pdr << " %\n";

        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalThroughput += throughputKbps;
    }

    double aggregatePdr = (totalTxPackets > 0)
                        ? (100.0 * totalRxPackets / totalTxPackets) : 0.0;
    double avgPerNodeThroughput =
        (nSenders > 0) ? (totalThroughput / nSenders) : 0.0;

    std::cout << "\n--- Aggregate ---\n"
              << "Total Tx Packets    : " << totalTxPackets   << "\n"
              << "Total Rx Packets    : " << totalRxPackets   << "\n"
              << "Aggregate Throughput: " << totalThroughput  << " Kbps\n"
              << "Per-Node Throughput : " << avgPerNodeThroughput << " Kbps\n"
              << "Aggregate PDR       : " << aggregatePdr     << " %\n";

    monitor->SerializeToXmlFile("task2-flowmon-" + std::to_string(nSenders) + ".xml",
                                true, true);

    Simulator::Destroy();
    return 0;
}
