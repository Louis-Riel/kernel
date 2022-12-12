class IRReceiver extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      ...props.config
    };
    if (!this.state.timing_groups) {
      this.state.timing_groups = this.getDefaultTimingGroup();
    }
  }

  buildTimingGroup(
    tag,
    carrier_freq_hz,
    duty_cycle,
    bit_length,
    invert,
    header_mark_us,
    header_space_us,
    one_mark_us,
    one_space_us,
    zero_mark_us,
    zero_space_us
  ) {
    return {
      tag: tag,
      carrier_freq_hz: carrier_freq_hz,
      duty_cycle: duty_cycle,
      bit_length: bit_length,
      invert: invert,
      header_mark_us: header_mark_us,
      header_space_us: header_space_us,
      one_mark_us: one_mark_us,
      one_space_us: one_space_us,
      zero_mark_us: zero_mark_us,
      zero_space_us: zero_space_us,
    };
  }

  getDefaultTimingGroup() {
    return [
      this.buildTimingGroup(
        "HiSense",
        38000,
        33,
        28,
        0,
        9000,
        4200,
        650,
        1600,
        650,
        450
      ),
      this.buildTimingGroup(
        "NEC",
        38000,
        33,
        32,
        0,
        9000,
        4250,
        560,
        1690,
        560,
        560
      ),
      this.buildTimingGroup(
        "LG",
        38000,
        33,
        28,
        0,
        9000,
        4200,
        550,
        1500,
        550,
        550
      ),
      this.buildTimingGroup(
        "samsung",
        38000,
        33,
        32,
        0,
        4600,
        4400,
        650,
        1500,
        553,
        453
      ),
      this.buildTimingGroup(
        "LG32",
        38000,
        33,
        32,
        0,
        4500,
        4500,
        500,
        1750,
        500,
        560
      ),
    ];
  }

  getTimingGroupTableHeader() {
    return e(
      MaterialUI.TableHead,
      { key: "IRTrackerTableHeader" },
      e(
        MaterialUI.TableRow,
        { key: "IRTrackerTableRow" },
        Object.keys(this.state.timing_groups[0]).map((key) =>
          e(MaterialUI.TableCell, { key: key }, key)
        )
      )
    );
  }
  updateTableProperty(event, record, field) {
    if (isNaN(event.target.value)) {
        record[field] = event.target.value;
    } else {
        record[field] = parseInt(event.target.value);
    }
    console.log(this.state);
  }

  getTimingGroupTableBody() {
    return e(
      MaterialUI.TableBody,
      { key: "IRTrackerTableBody" },
      this.state.timing_groups.map((group,idx) =>
        e(
          MaterialUI.TableRow,
          { key: group.tag + idx },
          Object.keys(group).map((key) =>
            e(MaterialUI.TableCell, { key: key },
                e(MaterialUI.TextField, {
                key: key,
                onChange: evt => this.updateTableProperty(evt, group, key),
                defaultValue: group[key]
            })
          )
        )
      )
    ));
  }

  getTimingGroupTable() {
    return e(
      MaterialUI.TableContainer,
      { key: "IRTrackerTableContainer" },
      e(MaterialUI.Table, { key: "IRTrackerTable" }, [
        this.getTimingGroupTableHeader(),
        this.getTimingGroupTableBody()
      ])
    );
  }

  updateProperty(event) {
    if (isNaN(event.target.value)) {
        this.state[event.target.labels[0].outerText] = event.target.value;
    } else {
        this.state[event.target.labels[0].outerText] = parseInt(event.target.value);
    }
  }

  Apply() {
    Object.keys(this.state).forEach((key) => {
        this.props.config[key] = this.state[key]
    });
    this.props.saveChanges();
  }

  render() {
    return e(
      MaterialUI.Card,
      { key: "IRTracker", variant: "outlined" },
      e(MaterialUI.CardContent, { key: "IRTrackerContent" }, [
        e(
          MaterialUI.Typography,
          { key: "IRTrackerTitle", variant: "h5" },
          e("span", { key: "IRTrackerTitleText" }, "IR Receiver"),
          e("span", { key: "filler", className:"filler" }),
          e(MaterialUI.Button, {key:"Apply", variant:"outlined", onClick: this.Apply.bind(this)}, "Apply")
        ),
        e(
          MaterialUI.Typography,
          { key: "IRTrackerSubtitle", variant: "body1" },
          "Configure the IR Receiver"
        ),
        e(MaterialUI.Divider, { key: "IRTrackerDivider" }),
        e(MaterialUI.CardContent, { key: "IRTrackerContent" }, [
          e(
            MaterialUI.Box,
            { key: "IRTrackerStack", className: "config-fields" },
            [
              Object.keys(this.state)
                    .filter((key) => !Array.isArray(this.state[key]))
                    .filter((key) => typeof this.state[key] !== "object")
                    .map((key) => {
                return e(MaterialUI.TextField, {
                  key: key,
                  label: key,
                  type: "number",
                  onChange: this.updateProperty.bind(this),
                  defaultValue: this.state[key],
                });
              }),
            ]
          ),
          e(
            MaterialUI.Box,
            { key: "IRTrackerStackTimingGroups", className: "timing-groups" },
            this.getTimingGroupTable()
          ),
        ]),
      ])
    );
  }
}
