// RemoteDevice / Conditions on a small hand-written device description (the file format of remotedevice.xml, no
// Access content): conditions, dump fields with masks, aliases and checksums, the update_dump fast path
#include "check.h"
#include "editor/editormodel.h"
#include "model/remotedevice.h"
#include <cstring>

using namespace vti;
using Bytes = std::vector<uint8_t>;

static const char *XML = R"(<?xml version="1.0"?>
<Remotedevice device="Test" version="1">
 <ParameterSets>
  <ParameterSet name="Osc">
   <Parameter name="Shape" minValue="0" maxValue="127" defaultValue="64" part="all"/>
   <Parameter name="Mode" minValue="0" maxValue="3" defaultValue="0" part="all"/>
   <Parameter name="Density" minValue="0" maxValue="127" defaultValue="0" part="all"/>
  </ParameterSet>
  <ParameterSet name="Global">
   <Parameter name="Panel" minValue="0" maxValue="15" defaultValue="0" part="none"/>
  </ParameterSet>
 </ParameterSets>
 <Conditionsets>
  <Condition name="Mode Is Saw"><Test type="equals" param="Osc/Mode" minvalue="1"/></Condition>
  <Condition name="Mode Is Saw"><Test type="equals" param="Osc/Mode" minvalue="3"/></Condition>
  <Condition name="Shape Middle And Mode 0" operand="and">
   <Test type="inside" param="Osc/Shape" minvalue="32" maxvalue="96"/>
   <Test type="equals" param="Osc/Mode" minvalue="0"/>
  </Condition>
  <Condition name="Shape Not Middle" result="invert">
   <Test type="inside" param="Osc/Shape" minvalue="32" maxvalue="96"/>
  </Condition>
  <Condition name="Shape Outside"><Test type="outside" param="Osc/Shape" minvalue="10" maxvalue="20"/></Condition>
  <Condition name="Panel Is 2"><Test type="equals" param="Global/Panel" minvalue="2"/></Condition>
 </Conditionsets>
 <MIDIMessages>
  <MIDIMessage id="Dump" name="Dump">
   <byte index="0" content="byte" tag="240"/>
   <byte index="1" content="Osc/Shape"/>
   <byte index="2" content="Osc/Mode" mask="3" shift="0"/>
   <byte index="2" content="Osc/Density" mask="124" shift="2"/>
   <byte index="3" content="Osc/Shape" part="1"/>
   <byte index="4" content="Osc/Density"/>
   <byte index="5" content="checksum" method="add7" startindex="1" stopindex="4"/>
   <byte index="6" content="byte" tag="247"/>
  </MIDIMessage>
 </MIDIMessages>
 <SoftknobMapping>
  <Mapping index="0" parameter="Dummy/Do Nothing"/>
  <Mapping index="1" parameter="Osc/Shape"/>
  <Mapping index="3" parameter="Osc/Density"/>
  <Mapping index="4" parameter="Osc/Gone"/>
 </SoftknobMapping>
</Remotedevice>)";

static RemoteDevice &device()
{
    static RemoteDevice d;
    static bool loaded = false;
    if (!loaded) {
        std::string err;
        Bytes x(XML, XML + strlen(XML));
        loaded = d.load(x, err);
        if (!loaded)
            std::fprintf(stderr, "  load: %s\n", err.c_str());
    }
    return d;
}

TEST(model_load)
{
    RemoteDevice &d = device();
    CHECK(d.find("Osc/Shape") && d.find("Global/Panel"));
    CHECK(!d.find("Osc/Nothing"));
    CHECK(d.message("Dump"));
}

TEST(model_softknob_targets)
{
    // a soft knob is a proxy of the parameter its destination selects (E150); 0 and unknown entries select nothing
    RemoteDevice &d = device();
    CHECK(d.softknob_parameter(1) == "Osc/Shape");
    CHECK(d.softknob_parameter(3) == "Osc/Density");
    CHECK(d.softknob_parameter(0).empty());  // "Do Nothing"
    CHECK(d.softknob_parameter(2).empty());  // a gap in the list
    CHECK(d.softknob_parameter(4).empty());  // a path that is no parameter of the device
    CHECK(d.softknob_parameter(5).empty());  // beyond the list
    CHECK(d.softknob_parameter(-1).empty());
}

TEST(conditions)
{
    RemoteDevice empty;
    Conditions c(device(), empty);
    std::map<std::string, int> v = {{"Osc/Shape", 64}, {"Osc/Mode", 0}, {"Global/Panel", 2}};
    int seen_part = -99;
    ValueFn f = [&](const std::string &p, int part) {
        if (p == "Global/Panel")
            seen_part = part;
        return v[p];
    };
    CHECK(c.eval("", f, 0));                     // no condition
    CHECK(c.eval("Unknown Condition", f, 0));    // unknown names count as true
    CHECK(!c.eval("Mode Is Saw", f, 0));
    v["Osc/Mode"] = 3;                           // the second alternative
    CHECK(c.eval("Mode Is Saw", f, 0));
    CHECK(!c.eval("Shape Middle And Mode 0", f, 0));
    v["Osc/Mode"] = 0;
    CHECK(c.eval("Shape Middle And Mode 0", f, 0));
    CHECK(!c.eval("Shape Not Middle", f, 0));
    v["Osc/Shape"] = 15;
    CHECK(c.eval("Shape Not Middle", f, 0) && !c.eval("Shape Outside", f, 0));
    CHECK(c.eval("Panel Is 2", f, 5) && seen_part == -1); // part "none": not part specific
    CHECK_EQ(c.default_value("Osc/Shape"), 64);
}

static Bytes blank_dump()
{
    Bytes m = {0xF0, 0, 0, 0, 0, 0, 0xF7};
    device().update_dump(*device().message("Dump"), m, {{"Osc/Shape", 0}, {"x/x", 0}}); // checksum
    return m;
}

TEST(dump_fields_and_checksum)
{
    const RemoteDevice &d = device();
    const MidiMessage &m = *d.message("Dump");
    CHECK_EQ(RemoteDevice::dump_size(m), 7u);
    Bytes msg = blank_dump();
    d.update_dump(m, msg, {{"Osc/Shape", 100}, {"Osc/Mode", 2}, {"Osc/Density", 9}, {"Osc/Shape@1", 7}});
    CHECK_EQ(msg[1], 100);
    CHECK_EQ(msg[2], 2 | 9 << 2); // two fields in one byte
    CHECK_EQ(msg[3], 7);
    CHECK_EQ(msg[4], 9);          // an alias: the same parameter in two places
    CHECK_EQ(msg[5], (100 + msg[2] + 7 + 9) & 0x7F);
    std::map<std::string, int> back;
    std::string err;
    CHECK(d.decode_dump(m, msg, back, err));
    CHECK(back["Osc/Shape"] == 100 && back["Osc/Mode"] == 2 && back["Osc/Density"] == 9 && back["Osc/Shape@1"] == 7);
    msg[5] ^= 1;
    CHECK(!d.decode_dump(m, msg, back, err)); // checksum
    CHECK(!d.decode_dump(m, Bytes(6, 0), back, err)); // length
    msg[5] ^= 1;
    msg[0] = 0xF1; // outside the checksum range: only the header check sees it
    CHECK(!d.decode_dump(m, msg, back, err) && err.find("header byte 0") != std::string::npos);
}

TEST(model_load_errors)
{
    RemoteDevice d;
    std::string err;
    const char *broken = "<Remotedevice><ParameterSets>";
    CHECK(!d.load(Bytes(broken, broken + strlen(broken)), err) && !err.empty());
    const char *other = "<?xml version=\"1.0\"?><Something/>";
    CHECK(!d.load(Bytes(other, other + strlen(other)), err) && err.find("no <Remotedevice>") != std::string::npos);
}

TEST(model_meta_clients_and_valuelist_text)
{
    static const char *xml = R"(<?xml version="1.0"?>
<Remotedevice device="Meta" version="1">
 <ParameterSets>
  <ParameterSet name="Macro">
   <Parameter name="Knob" minValue="0" maxValue="127" defaultValue="0" part="all" meta="true">
    <Client name="Osc/Shape" a="0.5" b="10" notrigger="true"/>
    <Client name="Osc/Mode" nosource="true"><Map clientval="1" metaval="0"/><Map clientval="3" metaval="127"/></Client>
   </Parameter>
  </ParameterSet>
 </ParameterSets>
 <Valuelists>
  <Valuelist name="Modes" type="plain"><Entry index="0">Off</Entry><Entry index="1">Saw</Entry></Valuelist>
 </Valuelists>
</Remotedevice>)";
    RemoteDevice d;
    std::string err;
    CHECK(d.load(Bytes(xml, xml + strlen(xml)), err));
    const Parameter *p = d.find("Macro/Knob");
    CHECK(p && p->meta && p->clients.size() == 2);
    if (p && p->clients.size() == 2) {
        const MetaClient &a = p->clients[0], &b = p->clients[1];
        CHECK(a.path == "Osc/Shape" && a.a == 0.5 && a.b == 10 && a.notrigger && !a.nosource && a.maps.empty());
        CHECK(b.path == "Osc/Mode" && b.a == 1 && b.b == 0 && !b.notrigger && b.nosource);
        CHECK(b.maps == (std::vector<std::pair<int, int>>{{1, 0}, {3, 127}}));
    }
    const Valuelist &l = d.valuelists["Modes"];
    CHECK(l.text(1) == "Saw" && l.text(7) == "7"); // an unknown value shows as its number
}

TEST(dump_fast_path_equals_full_path)
{
    const RemoteDevice &d = device();
    const MidiMessage &m = *d.message("Dump");
    for (const char *key : {"Osc/Shape", "Osc/Mode", "Osc/Density", "Osc/Shape@1", "Not/There"})
        for (int v : {0, 1, 3, 31, 64, 127}) {
            Bytes a = blank_dump(), b = a;
            d.update_dump(m, a, {{key, v}});               // one value: the indexed fast path
            d.update_dump(m, b, {{key, v}, {"zz/none", 0}}); // two: the full walk
            CHECK(a == b);
        }
}

// a parameter SysEx template with every transport byte kind; `tag` sets its fixed byte, `map` the controller index
static bool load_transport(RemoteDevice &d, const char *tag, const char *map)
{
    std::string xml = std::string(R"(<?xml version="1.0"?><Remotedevice device="T" version="1"><ParameterSets>
 <ParameterSet name="Osc"><Parameter name="Shape" part="all" defaultmessage="P"/></ParameterSet></ParameterSets>
 <MIDIMessages><MIDIMessage id="P" name="P">
  <byte index="0" content="sysexstart"/><byte index="1" content="byte" tag=")") + tag + R"("/>
  <byte index="2" content="deviceid"/><byte index="3" content="part"/>
  <byte index="4" content="controllerid"><parameter mapindex=")" + map + R"(" content="Osc/Shape"/></byte>
  <byte index="5" content="value"/><byte index="6" content="sysexend"/>
 </MIDIMessage></MIDIMessages></Remotedevice>)";
    std::string err;
    return d.load(Bytes(xml.begin(), xml.end()), err);
}

// nothing outside 7 bits reaches the wire as a data byte (review B1): such a message is not built at all
TEST(model_encode_refuses_bytes_above_7f)
{
    RemoteDevice d;
    CHECK(load_transport(d, "114", "5"));
    uint8_t m[16];
    CHECK_EQ(d.encode_into("Osc/Shape", 3, 2, m, sizeof m, 0x10), 7u);
    CHECK(m[1] == 114 && m[2] == 0x10 && m[3] == 2 && m[4] == 5 && m[5] == 3 && m[6] == 0xF7);
    CHECK_EQ(d.encode_into("Osc/Shape", 3, 2, m, sizeof m, 0x80), 0u);  // device id
    CHECK_EQ(d.encode_into("Osc/Shape", 3, 0x80, m, sizeof m, 0x10), 0u); // part
    CHECK_EQ(d.encode_into("Osc/Shape", 3, -1, m, sizeof m, 0x10), 0u);
    RemoteDevice c;
    CHECK(load_transport(c, "114", "200"));
    CHECK_EQ(c.encode_into("Osc/Shape", 3, 2, m, sizeof m, 0x10), 0u);   // controller id
    RemoteDevice t;
    CHECK(!load_transport(t, "200", "5"));                               // a fixed byte that is no MIDI byte
    CHECK(!load_transport(t, "300", "5"));
}
