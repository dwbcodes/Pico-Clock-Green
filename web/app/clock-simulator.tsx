import type { DisplayStatus } from "./contracts";

type FrontButton = "next" | "up" | "down";

function framePixel(display: DisplayStatus | null, x: number, y: number) {
  const row = display?.frame?.[y];
  if (!row || !/^[0-9a-f]{6}$/i.test(row)) return false;
  return (Number.parseInt(row, 16) & (1 << (23 - x))) !== 0;
}

export function ClockSimulator({
  display,
  busy,
  onButton,
}: {
  display: DisplayStatus | null;
  busy: boolean;
  onButton: (button: FrontButton) => void;
}) {
  const weekdayColumns = [[3, 4], [6, 7], [9, 10], [12, 13], [15, 16], [18, 19], [21, 22]];
  const weekdayLabels = ["Mon.", "Tue.", "Wed.", "Thu.", "Fri.", "Sat.", "Sun."];
  return (
    <section className="clock-simulator" aria-label="Live greenpico emulator">
      <div className="clock-topline">
        <div><p className="eyebrow">Live device</p><h2>greenpico display</h2></div>
        <span className="page-chip">{display?.page ?? "connecting"}</span>
      </div>
      <div className="clock-hardware">
        <div className={`led-window ${display?.powerOverride === "off-until-schedule" ? "sleeping" : ""}`}>
          <div className="weekday-lights" aria-hidden="true">
            {weekdayLabels.map((day, index) => (
              <span key={`${day}-${index}`} className={weekdayColumns[index].some((x) => framePixel(display, x, 0)) ? "lit" : ""}>{day}</span>
            ))}
          </div>
          <div className="led-face">
            <div className="indicator-rail" aria-label="Display indicators">
              <span className={framePixel(display, 0, 3) ? "lit" : ""}>F</span>
              <span className={framePixel(display, 1, 3) ? "lit" : ""}>C</span>
              <span className={framePixel(display, 0, 4) ? "lit" : ""}>AM</span>
              <span className={framePixel(display, 1, 4) ? "lit" : ""}>PM</span>
              <span className={framePixel(display, 0, 6) || framePixel(display, 1, 6) ? "lit wide" : "wide"}>CHIME</span>
              <span className={framePixel(display, 0, 7) || framePixel(display, 1, 7) ? "lit wide" : "wide"}>AUTO LIGHT</span>
            </div>
            <div className="dot-matrix" aria-hidden="true">
              {Array.from({ length: 7 }, (_, row) => {
                const y = row + 1;
                return Array.from({ length: 22 }, (_, column) => {
                  const x = column + 2;
                  const classes = [
                    framePixel(display, x, y) ? "lit" : "",
                    display?.page === "time" && (x === 12 || x === 13) ? "seconds-pixel" : "",
                    [6, 11, 14, 19].includes(x) ? "digit-gap" : "",
                  ].filter(Boolean).join(" ");
                  return <i key={`${x}-${y}`} className={classes} />;
                });
              })}
            </div>
          </div>
          <output className="sr-only" aria-live="polite">Display page: {display?.page ?? "connecting"}</output>
          <div className="led-detail">API framebuffer · {display?.brightnessPercent ?? 0}% brightness</div>
        </div>
        <div className="front-controls" aria-label="Clock front-panel buttons">
          <button type="button" disabled={busy} onClick={() => onButton("next")}><b>NEXT</b><small>Next page</small></button>
          <button type="button" disabled={busy} onClick={() => onButton("up")}><b>▲</b><small>Up</small></button>
          <button type="button" disabled={busy} onClick={() => onButton("down")}><b>▼</b><small>Down</small></button>
        </div>
      </div>
      <p className="clock-hint">The controls call the same API actions as the physical NEXT, UP, and DOWN buttons.</p>
    </section>
  );
}
