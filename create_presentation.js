const PptxGenJS = require("pptxgenjs");

// Create presentation
const prs = new PptxGenJS();
prs.defineLayout({ name: "LAYOUT1", width: 10, height: 7.5 });
prs.defineLayout({ name: "LAYOUT2", width: 10, height: 7.5 });

// Color palette - Deep Blue & Teal theme (safety/technology focused)
const colors = {
  dark: "1E2761",      // Navy blue
  primary: "065A82",   // Deep blue
  secondary: "00A896", // Teal
  accent: "F96167",    // Coral (warnings)
  light: "F5F5F5",     // Off-white
  text: "FFFFFF",      // White
  darkText: "1E2761",  // Navy for light backgrounds
};

const defaultProps = {
  theme: {
    name: "Office",
    colorScheme: "Office",
  },
};

// Helper functions
function addTitleSlide(title, subtitle) {
  const slide = prs.addSlide();
  slide.background = { color: colors.dark };

  // Title
  slide.addText(title, {
    x: 0.5, y: 2.5, w: 9, h: 1,
    fontSize: 54, bold: true, color: colors.text,
    fontFace: "Cambria", align: "left"
  });

  // Subtitle
  if (subtitle) {
    slide.addText(subtitle, {
      x: 0.5, y: 3.7, w: 9, h: 1.5,
      fontSize: 24, color: colors.secondary,
      fontFace: "Calibri", align: "left"
    });
  }

  // Decorative line
  slide.addShape(prs.ShapeType.rect, {
    x: 0.5, y: 1.8, w: 2, h: 0.1,
    fill: { color: colors.secondary }, line: { type: "none" }
  });
}

function addContentSlide(title, content) {
  const slide = prs.addSlide();
  slide.background = { color: colors.light };

  // Header bar
  slide.addShape(prs.ShapeType.rect, {
    x: 0, y: 0, w: 10, h: 0.8,
    fill: { color: colors.primary }, line: { type: "none" }
  });

  // Title
  slide.addText(title, {
    x: 0.5, y: 0.15, w: 9, h: 0.5,
    fontSize: 40, bold: true, color: colors.text,
    fontFace: "Cambria", align: "left"
  });

  // Content
  let yPos = 1.2;
  content.forEach((item, index) => {
    if (item.type === "heading") {
      slide.addText(item.text, {
        x: 0.5, y: yPos, w: 9, h: 0.4,
        fontSize: 18, bold: true, color: colors.primary,
        fontFace: "Cambria"
      });
      yPos += 0.5;
    } else if (item.type === "bullet") {
      // Icon circle
      slide.addShape(prs.ShapeType.ellipse, {
        x: 0.5, y: yPos + 0.1, w: 0.25, h: 0.25,
        fill: { color: colors.secondary }, line: { type: "none" }
      });

      // Text
      slide.addText(item.text, {
        x: 1, y: yPos, w: 8.5, h: 0.4,
        fontSize: 14, color: colors.darkText,
        fontFace: "Calibri"
      });
      yPos += 0.6;
    } else if (item.type === "body") {
      slide.addText(item.text, {
        x: 0.5, y: yPos, w: 9, h: 0.5,
        fontSize: 13, color: colors.darkText,
        fontFace: "Calibri"
      });
      yPos += 0.7;
    }
  });

  return slide;
}

function addTwoColumnSlide(title, leftTitle, leftContent, rightTitle, rightContent) {
  const slide = prs.addSlide();
  slide.background = { color: colors.light };

  // Header
  slide.addShape(prs.ShapeType.rect, {
    x: 0, y: 0, w: 10, h: 0.8,
    fill: { color: colors.primary }, line: { type: "none" }
  });

  slide.addText(title, {
    x: 0.5, y: 0.15, w: 9, h: 0.5,
    fontSize: 40, bold: true, color: colors.text,
    fontFace: "Cambria"
  });

  // Left column
  slide.addText(leftTitle, {
    x: 0.5, y: 1.1, w: 4.3, h: 0.4,
    fontSize: 16, bold: true, color: colors.primary,
    fontFace: "Cambria"
  });

  let yPos = 1.6;
  leftContent.forEach(text => {
    slide.addText("• " + text, {
      x: 0.7, y: yPos, w: 3.9, h: 0.5,
      fontSize: 12, color: colors.darkText,
      fontFace: "Calibri"
    });
    yPos += 0.6;
  });

  // Right column
  slide.addText(rightTitle, {
    x: 5.2, y: 1.1, w: 4.3, h: 0.4,
    fontSize: 16, bold: true, color: colors.primary,
    fontFace: "Cambria"
  });

  yPos = 1.6;
  rightContent.forEach(text => {
    slide.addText("• " + text, {
      x: 5.4, y: yPos, w: 3.9, h: 0.5,
      fontSize: 12, color: colors.darkText,
      fontFace: "Calibri"
    });
    yPos += 0.6;
  });

  return slide;
}

// ===== SLIDES =====

// Slide 1: Title
addTitleSlide(
  "Elevator Control System",
  "Design with Safety Guardrails"
);

// Slide 2: Agenda
addContentSlide("Agenda", [
  { type: "bullet", text: "System Overview & Architecture" },
  { type: "bullet", text: "State Machine Design" },
  { type: "bullet", text: "Guardrails Concept & Implementation" },
  { type: "bullet", text: "Safety Mechanisms & Detection" },
  { type: "bullet", text: "Testing & Verification" },
  { type: "bullet", text: "Demonstration & Results" },
]);

// Slide 3: System Overview
addTwoColumnSlide(
  "System Overview",
  "What We're Building",
  [
    "Smart elevator controller",
    "10 floors (0-9)",
    "Smooth motion with acceleration",
    "Door control automation",
    "Emergency stop capability"
  ],
  "Why It Matters",
  [
    "Safety-critical system",
    "Multiple failure modes",
    "Real-time constraints",
    "User interaction",
    "Predictable behavior"
  ]
);

// Slide 4: Architecture
const slide4 = prs.addSlide();
slide4.background = { color: colors.light };
slide4.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.primary }, line: { type: "none" }
});
slide4.addText("System Architecture", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

// Component boxes
const components = [
  { title: "Elevator\nModule", desc: "State machine\n11 states", x: 0.5, color: colors.secondary },
  { title: "Button\nManager", desc: "Track 10 floors\nQueuing logic", x: 2.5, color: colors.primary },
  { title: "Motion\nManager", desc: "Smooth transitions\nFloor tracking", x: 4.5, color: colors.secondary },
  { title: "Door\nManager", desc: "Opening/closing\nAnimation", x: 6.5, color: colors.primary },
  { title: "Guardrails\nModule", desc: "Safety checks\nViolation enforce", x: 8.5, color: colors.accent },
];

components.forEach(comp => {
  slide4.addShape(prs.ShapeType.rect, {
    x: comp.x, y: 1.5, w: 1.8, h: 2.5,
    fill: { color: comp.color }, line: { color: "CCCCCC", width: 2 }
  });

  slide4.addText(comp.title, {
    x: comp.x, y: 1.7, w: 1.8, h: 0.6,
    fontSize: 13, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center"
  });

  slide4.addText(comp.desc, {
    x: comp.x, y: 2.5, w: 1.8, h: 1.3,
    fontSize: 11, color: colors.text,
    fontFace: "Calibri", align: "center"
  });
});

// State count
slide4.addShape(prs.ShapeType.rect, {
  x: 1, y: 4.5, w: 8, h: 1.2,
  fill: { color: colors.light }, line: { color: colors.primary, width: 2 }
});
slide4.addText("11 States | 3 Safety Checks | 49 Unit Tests | 100% Pass Rate", {
  x: 1.2, y: 4.65, w: 7.6, h: 0.8,
  fontSize: 16, bold: true, color: colors.primary,
  fontFace: "Calibri", align: "center"
});

// Slide 5: State Machine Intro
addContentSlide("State Machine: The Heart", [
  { type: "heading", text: "What is a State Machine?" },
  { type: "bullet", text: "System with 11 distinct states" },
  { type: "bullet", text: "Clear transitions between states" },
  { type: "bullet", text: "Every action defined by current state" },
  { type: "heading", text: "Key Benefit" },
  { type: "bullet", text: "Impossible to enter invalid state combinations" },
  { type: "bullet", text: "Predictable, deterministic behavior" },
]);

// Slide 6: State Diagram Overview
const slide6 = prs.addSlide();
slide6.background = { color: colors.light };
slide6.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.primary }, line: { type: "none" }
});
slide6.addText("11 Elevator States", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

// State circles
const states = [
  { name: "IDLE", type: "floor" },
  { name: "SMOOTH_\nSTART_UP", type: "motion" },
  { name: "MOVING_UP", type: "motion" },
  { name: "SMOOTH_\nSTOP_UP", type: "motion" },
  { name: "DOORS_\nOPENING", type: "door" },
  { name: "DOORS_OPEN", type: "door" },
];

const typeColors = {
  "floor": colors.secondary,
  "motion": colors.primary,
  "door": colors.accent,
};

let xStart = 0.5;
states.forEach((state, i) => {
  const radius = 0.6;
  const color = typeColors[state.type];

  slide6.addShape(prs.ShapeType.ellipse, {
    x: xStart, y: 2, w: 1.4, h: 1.4,
    fill: { color: color }, line: { type: "none" }
  });

  slide6.addText(state.name, {
    x: xStart, y: 2.3, w: 1.4, h: 0.8,
    fontSize: 10, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center"
  });

  xStart += 1.55;
});

// Legend
let legendY = 4;
[
  { type: "floor", label: "At Floor" },
  { type: "motion", label: "In Motion" },
  { type: "door", label: "Door Operation" },
].forEach(item => {
  slide6.addShape(prs.ShapeType.ellipse, {
    x: 0.7, y: legendY, w: 0.3, h: 0.3,
    fill: { color: typeColors[item.type] }, line: { type: "none" }
  });

  slide6.addText(item.label, {
    x: 1.2, y: legendY, w: 2, h: 0.35,
    fontSize: 12, color: colors.darkText,
    fontFace: "Calibri"
  });

  legendY += 0.5;
});

// Slide 7: State Machine Details
addTwoColumnSlide(
  "State Transitions",
  "Smooth Motion States",
  [
    "SMOOTH_START_UP (1 tick)",
    "MOVING_UP (5 ticks/floor)",
    "SMOOTH_STOP_UP (1 tick)",
    "Provides natural acceleration",
    "Realistic motion profile"
  ],
  "Door Operation States",
  [
    "DOORS_OPENING (2 ticks)",
    "DOORS_OPEN (10 ticks)",
    "DOORS_CLOSING (2 ticks)",
    "Button cleared automatically",
    "Smooth user experience"
  ]
);

// Slide 8: What are Guardrails?
addContentSlide("Guardrails: Safety Net", [
  { type: "heading", text: "Definition" },
  { type: "bullet", text: "Additional safety layer on top of state machine" },
  { type: "bullet", text: "Detect violations → Enforce corrective action" },
  { type: "heading", text: "Why Needed?" },
  { type: "bullet", text: "State machine defines valid behavior" },
  { type: "bullet", text: "Guardrails catch unexpected situations" },
  { type: "bullet", text: "Defense-in-depth approach" },
]);

// Slide 9: Three Guardrails
const slide9 = prs.addSlide();
slide9.background = { color: colors.light };
slide9.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.primary }, line: { type: "none" }
});
slide9.addText("The 3 Safety Guardrails", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

const guardrails = [
  {
    num: "1",
    title: "Doors Open While Moving",
    desc: "CRITICAL: Detect when doors are open while elevator is in motion",
    action: "Force immediate stop and close doors",
    severity: colors.accent,
  },
  {
    num: "2",
    title: "Emergency Stop Between Floors",
    desc: "SAFETY: Prevent stopping elevator between floors with people",
    action: "Force movement to nearest floor",
    severity: colors.accent,
  },
  {
    num: "3",
    title: "Stop Button Tracking",
    desc: "INFO: Monitor emergency stop duration and prompt operator",
    action: "Prompt every 20 seconds of inactivity",
    severity: colors.secondary,
  },
];

let yPos = 1.3;
guardrails.forEach(gr => {
  // Number circle
  slide9.addShape(prs.ShapeType.ellipse, {
    x: 0.5, y: yPos, w: 0.4, h: 0.4,
    fill: { color: gr.severity }, line: { type: "none" }
  });

  slide9.addText(gr.num, {
    x: 0.5, y: yPos, w: 0.4, h: 0.4,
    fontSize: 16, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center", valign: "middle"
  });

  // Title
  slide9.addText(gr.title, {
    x: 1.1, y: yPos, w: 8.4, h: 0.35,
    fontSize: 13, bold: true, color: colors.darkText,
    fontFace: "Cambria"
  });

  // Description
  slide9.addText(gr.desc, {
    x: 1.1, y: yPos + 0.35, w: 8.4, h: 0.4,
    fontSize: 11, color: colors.darkText,
    fontFace: "Calibri"
  });

  // Action
  slide9.addText("→ Action: " + gr.action, {
    x: 1.1, y: yPos + 0.75, w: 8.4, h: 0.3,
    fontSize: 11, italic: true, color: colors.primary,
    fontFace: "Calibri"
  });

  yPos += 1.5;
});

// Slide 10: Violation 1 - Doors Open
const slide10 = prs.addSlide();
slide10.background = { color: colors.light };
slide10.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.accent }, line: { type: "none" }
});
slide10.addText("Guardrail #1: Doors Open While Moving", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

slide10.addText("The Problem", {
  x: 0.5, y: 1.1, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.accent,
  fontFace: "Cambria"
});

slide10.addText("Passenger or object could fall through open doors while elevator moves between floors", {
  x: 0.7, y: 1.5, w: 8.6, h: 0.6,
  fontSize: 13, color: colors.darkText,
  fontFace: "Calibri"
});

slide10.addText("How We Detect It", {
  x: 0.5, y: 2.3, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.primary,
  fontFace: "Cambria"
});

slide10.addText("Check on every tick: IF doors_open AND elevator_is_between_floors THEN VIOLATION", {
  x: 0.7, y: 2.75, w: 8.6, h: 0.6,
  fontSize: 12, color: colors.darkText, italic: true,
  fontFace: "Calibri"
});

slide10.addText("Enforcement Action", {
  x: 0.5, y: 3.6, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.secondary,
  fontFace: "Cambria"
});

const actions1 = [
  "1. Immediately set state to IDLE",
  "2. Doors close automatically",
  "3. Motion stops",
  "4. Elevator settles at safe floor"
];

let actionY = 4.05;
actions1.forEach((action, i) => {
  slide10.addText(action, {
    x: 0.7, y: actionY, w: 8.6, h: 0.35,
    fontSize: 12, color: colors.darkText,
    fontFace: "Calibri"
  });
  actionY += 0.45;
});

// Slide 11: Violation 2 - Between Floors
const slide11 = prs.addSlide();
slide11.background = { color: colors.light };
slide11.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.accent }, line: { type: "none" }
});
slide11.addText("Guardrail #2: Emergency Stop Between Floors", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

slide11.addText("The Problem", {
  x: 0.5, y: 1.1, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.accent,
  fontFace: "Cambria"
});

slide11.addText("Emergency stop button pressed while elevator is suspended between floors with trapped passengers", {
  x: 0.7, y: 1.5, w: 8.6, h: 0.7,
  fontSize: 13, color: colors.darkText,
  fontFace: "Calibri"
});

slide11.addText("How We Prevent It", {
  x: 0.5, y: 2.4, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.primary,
  fontFace: "Cambria"
});

slide11.addText("Detect: EMERGENCY_STOP state AND elevator_is_between_floors", {
  x: 0.7, y: 2.85, w: 8.6, h: 0.5,
  fontSize: 12, color: colors.darkText, italic: true,
  fontFace: "Calibri"
});

slide11.addText("Smart Enforcement", {
  x: 0.5, y: 3.5, w: 9, h: 0.35,
  fontSize: 16, bold: true, color: colors.secondary,
  fontFace: "Cambria"
});

const actions2 = [
  "If in LOWER HALF (floors 0-4): Force movement UP to next floor",
  "If in UPPER HALF (floors 5-9): Force movement DOWN to next floor",
  "Resume normal motion: Get to nearest safe floor → Stop → Open doors"
];

let actionY2 = 3.95;
actions2.forEach((action, i) => {
  slide11.addText((i+1) + ". " + action, {
    x: 0.7, y: actionY2, w: 8.6, h: 0.5,
    fontSize: 12, color: colors.darkText,
    fontFace: "Calibri"
  });
  actionY2 += 0.65;
});

// Slide 12: How Guardrails Work
const slide12 = prs.addSlide();
slide12.background = { color: colors.light };
slide12.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.primary }, line: { type: "none" }
});
slide12.addText("Guardrail Execution Flow", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

// Flow boxes
const flowSteps = [
  { num: "1", text: "Check for\nViolations", color: colors.secondary },
  { num: "2", text: "Detect\nProblem", color: colors.accent },
  { num: "3", text: "Enforce\nSafety", color: colors.primary },
  { num: "4", text: "Report\nStatus", color: colors.secondary },
];

let flowX = 0.7;
flowSteps.forEach((step, i) => {
  // Box
  slide12.addShape(prs.ShapeType.rect, {
    x: flowX, y: 1.8, w: 2, h: 1.2,
    fill: { color: step.color }, line: { type: "none" }
  });

  // Number
  slide12.addText(step.num, {
    x: flowX, y: 1.95, w: 2, h: 0.3,
    fontSize: 18, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center"
  });

  // Text
  slide12.addText(step.text, {
    x: flowX + 0.1, y: 2.35, w: 1.8, h: 0.6,
    fontSize: 12, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center"
  });

  // Arrow
  if (i < flowSteps.length - 1) {
    slide12.addShape(prs.ShapeType.triangle, {
      x: flowX + 2.1, y: 2.25, w: 0.35, h: 0.35,
      fill: { color: colors.primary }, line: { type: "none" },
      rotate: 90
    });
  }

  flowX += 2.35;
});

// Execution details
const details = [
  "guardrail_check(&elevator) → Returns GuardrailStatus",
  "guardrail_enforce(&elevator) → Modifies state if needed",
  "guardrail_violation_description() → Generate message",
];

let detailY = 3.4;
details.forEach(detail => {
  slide12.addText("▸ " + detail, {
    x: 0.7, y: detailY, w: 8.6, h: 0.4,
    fontSize: 11, color: colors.darkText,
    fontFace: "Calibri"
  });
  detailY += 0.55;
});

slide12.addShape(prs.ShapeType.rect, {
  x: 0.5, y: 5.2, w: 9, h: 1.3,
  fill: { color: colors.light }, line: { color: colors.primary, width: 2 }
});

slide12.addText("Every Tick (100ms): Check violations → Enforce → Execute state → Advance time", {
  x: 0.7, y: 5.35, w: 8.6, h: 1,
  fontSize: 13, bold: true, color: colors.primary,
  fontFace: "Calibri", align: "center", valign: "middle"
});

// Slide 13: Testing
addContentSlide("Testing & Verification", [
  { type: "heading", text: "Comprehensive Test Suite" },
  { type: "bullet", text: "49 total unit tests" },
  { type: "bullet", text: "28 elevator state machine tests" },
  { type: "bullet", text: "21 guardrails safety tests" },
  { type: "heading", text: "Coverage" },
  { type: "bullet", text: "All 11 states tested" },
  { type: "bullet", text: "All 3 guardrail violations verified" },
  { type: "bullet", text: "Edge cases and boundaries" },
]);

// Slide 14: Test Results
const slide14 = prs.addSlide();
slide14.background = { color: colors.light };
slide14.addShape(prs.ShapeType.rect, {
  x: 0, y: 0, w: 10, h: 0.8,
  fill: { color: colors.primary }, line: { type: "none" }
});
slide14.addText("Test Results", {
  x: 0.5, y: 0.15, w: 9, h: 0.5,
  fontSize: 40, bold: true, color: colors.text,
  fontFace: "Cambria"
});

// Large stats
const stats = [
  { label: "Total Tests", value: "49", x: 0.5 },
  { label: "Passed", value: "49", x: 3.5 },
  { label: "Failed", value: "0", x: 6.5 },
];

stats.forEach(stat => {
  // Background
  slide14.addShape(prs.ShapeType.rect, {
    x: stat.x, y: 1.5, w: 2.8, h: 2,
    fill: { color: colors.secondary }, line: { type: "none" }
  });

  // Number
  slide14.addText(stat.value, {
    x: stat.x, y: 1.8, w: 2.8, h: 0.8,
    fontSize: 48, bold: true, color: colors.text,
    fontFace: "Calibri", align: "center"
  });

  // Label
  slide14.addText(stat.label, {
    x: stat.x, y: 2.7, w: 2.8, h: 0.6,
    fontSize: 14, color: colors.text,
    fontFace: "Calibri", align: "center"
  });
});

// Success rate
slide14.addShape(prs.ShapeType.rect, {
  x: 1.5, y: 4, w: 7, h: 1.5,
  fill: { color: "#E8F5E9" }, line: { color: colors.secondary, width: 3 }
});

slide14.addText("SUCCESS RATE", {
  x: 1.7, y: 4.15, w: 6.6, h: 0.35,
  fontSize: 14, bold: true, color: colors.primary,
  fontFace: "Cambria", align: "center"
});

slide14.addText("100.0%", {
  x: 1.7, y: 4.55, w: 6.6, h: 0.7,
  fontSize: 52, bold: true, color: colors.secondary,
  fontFace: "Calibri", align: "center"
});

// Slide 15: Key Takeaways
const slide15 = prs.addSlide();
slide15.background = { color: colors.dark };

slide15.addText("Key Takeaways", {
  x: 0.5, y: 0.5, w: 9, h: 0.6,
  fontSize: 44, bold: true, color: colors.secondary,
  fontFace: "Cambria"
});

const takeaways = [
  "State machines provide safe, predictable behavior",
  "Guardrails add defense-in-depth safety layer",
  "Detect violations → Enforce corrective action",
  "Three specific safety mechanisms prevent accidents",
  "Comprehensive testing verifies all scenarios",
  "100% test pass rate confirms system reliability",
];

let takeY = 1.5;
takeaways.forEach((takeaway, i) => {
  slide15.addShape(prs.ShapeType.ellipse, {
    x: 0.5, y: takeY + 0.12, w: 0.3, h: 0.3,
    fill: { color: colors.secondary }, line: { type: "none" }
  });

  slide15.addText(takeaway, {
    x: 1, y: takeY, w: 8.5, h: 0.5,
    fontSize: 14, color: colors.text,
    fontFace: "Calibri"
  });

  takeY += 0.8;
});

// Slide 16: Conclusion
const slide16 = prs.addSlide();
slide16.background = { color: colors.primary };

slide16.addText("Safe. Reliable. Predictable.", {
  x: 0.5, y: 2, w: 9, h: 1,
  fontSize: 48, bold: true, color: colors.text,
  fontFace: "Cambria", align: "center"
});

slide16.addText("An elevator system that prioritizes passenger safety through intelligent state management and layered guardrails.", {
  x: 1, y: 3.2, w: 8, h: 1.2,
  fontSize: 18, color: colors.text,
  fontFace: "Calibri", align: "center"
});

slide16.addShape(prs.ShapeType.rect, {
  x: 2, y: 4.8, w: 6, h: 0.08,
  fill: { color: colors.secondary }, line: { type: "none" }
});

// Save
prs.writeFile("Elevator_Design_Presentation.pptx");
console.log("Presentation created successfully!");
