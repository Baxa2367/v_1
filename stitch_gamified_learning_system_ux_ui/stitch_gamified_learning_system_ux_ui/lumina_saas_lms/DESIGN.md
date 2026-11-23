---
name: Lumina SaaS LMS
colors:
  surface: '#f8f9ff'
  surface-dim: '#cbdbf5'
  surface-bright: '#f8f9ff'
  surface-container-lowest: '#ffffff'
  surface-container-low: '#eff4ff'
  surface-container: '#e5eeff'
  surface-container-high: '#dce9ff'
  surface-container-highest: '#d3e4fe'
  on-surface: '#0b1c30'
  on-surface-variant: '#464555'
  inverse-surface: '#213145'
  inverse-on-surface: '#eaf1ff'
  outline: '#777587'
  outline-variant: '#c7c4d8'
  surface-tint: '#4d44e3'
  primary: '#3525cd'
  on-primary: '#ffffff'
  primary-container: '#4f46e5'
  on-primary-container: '#dad7ff'
  inverse-primary: '#c3c0ff'
  secondary: '#006c49'
  on-secondary: '#ffffff'
  secondary-container: '#6cf8bb'
  on-secondary-container: '#00714d'
  tertiary: '#684000'
  on-tertiary: '#ffffff'
  tertiary-container: '#885500'
  on-tertiary-container: '#ffd4a4'
  error: '#ba1a1a'
  on-error: '#ffffff'
  error-container: '#ffdad6'
  on-error-container: '#93000a'
  primary-fixed: '#e2dfff'
  primary-fixed-dim: '#c3c0ff'
  on-primary-fixed: '#0f0069'
  on-primary-fixed-variant: '#3323cc'
  secondary-fixed: '#6ffbbe'
  secondary-fixed-dim: '#4edea3'
  on-secondary-fixed: '#002113'
  on-secondary-fixed-variant: '#005236'
  tertiary-fixed: '#ffddb8'
  tertiary-fixed-dim: '#ffb95f'
  on-tertiary-fixed: '#2a1700'
  on-tertiary-fixed-variant: '#653e00'
  background: '#f8f9ff'
  on-background: '#0b1c30'
  surface-variant: '#d3e4fe'
typography:
  display-lg:
    fontFamily: Plus Jakarta Sans
    fontSize: 48px
    fontWeight: '800'
    lineHeight: 56px
    letterSpacing: -0.03em
  display-lg-mobile:
    fontFamily: Plus Jakarta Sans
    fontSize: 32px
    fontWeight: '800'
    lineHeight: 40px
    letterSpacing: -0.02em
  headline-lg:
    fontFamily: Plus Jakarta Sans
    fontSize: 32px
    fontWeight: '700'
    lineHeight: 40px
    letterSpacing: -0.02em
  headline-lg-mobile:
    fontFamily: Plus Jakarta Sans
    fontSize: 24px
    fontWeight: '700'
    lineHeight: 32px
    letterSpacing: -0.01em
  headline-md:
    fontFamily: Plus Jakarta Sans
    fontSize: 24px
    fontWeight: '600'
    lineHeight: 32px
    letterSpacing: -0.015em
  headline-sm:
    fontFamily: Plus Jakarta Sans
    fontSize: 18px
    fontWeight: '600'
    lineHeight: 26px
    letterSpacing: -0.01em
  body-lg:
    fontFamily: Plus Jakarta Sans
    fontSize: 16px
    fontWeight: '400'
    lineHeight: 26px
  body-md:
    fontFamily: Plus Jakarta Sans
    fontSize: 14px
    fontWeight: '400'
    lineHeight: 22px
  body-sm:
    fontFamily: Plus Jakarta Sans
    fontSize: 12px
    fontWeight: '400'
    lineHeight: 18px
  label-md:
    fontFamily: JetBrains Mono
    fontSize: 12px
    fontWeight: '500'
    lineHeight: 16px
    letterSpacing: 0.02em
  label-sm:
    fontFamily: JetBrains Mono
    fontSize: 10px
    fontWeight: '500'
    lineHeight: 14px
    letterSpacing: 0.04em
rounded:
  sm: 0.25rem
  DEFAULT: 0.5rem
  md: 0.75rem
  lg: 1rem
  xl: 1.5rem
  full: 9999px
spacing:
  gutter: 1.5rem
  gutter-mobile: 0.75rem
  margin: 2rem
  margin-mobile: 1rem
  space-xs: 0.25rem
  space-sm: 0.5rem
  space-md: 1rem
  space-lg: 1.5rem
  space-xl: 2.5rem
---

## Brand & Style

This design system establishes a high-performance, mature gamified SaaS ecosystem that balances the crisp, disciplined precision of Linear with the kinetic delight of Duolingo. It is tailored for high-achieving students, enterprise educators, and analytical institutional administrators who require trilingual interfaces (Kazakh, Russian, and English). 

The aesthetic is clean, modern, and intellectual. Rather than juvenile or cartoonish gamification, the visual narrative treats mastery, streaks, and knowledge acquisition as refined crafts. It conveys momentum, analytical clarity, and focus through airy surfaces, micro-textured borders, sharp metric visualizations, and vibrant chromatic reward nodes. 

Visual principles:
- **Calibrated Vibrancy:** Gamification signals (emerald completions, amber streaks, cyan AI assists) stand out crisply against quiet, slate-tinted canvases.
- **Data-Dense Yet Breathable:** Modular workspaces allow detailed progress charts and interactive AI conversations without visual clutter.
- **International Typographic Balance:** Cyrillic and Latin glyph sets exhibit identical visual weights and optical balance across all scripts.

## Colors

The palette operates on a crisp, clinical baseline energized by functional chromatic accents:

- **Primary (`#4F46E5`, `#4338CA`):** Deep indigo/blue-violet anchors system hierarchy, primary actions, selected states, and structural brand moments.
- **Secondary (`#10B981`):** Vivid emerald green signals progress fulfillment, validated masteries, correct quiz inputs, and positive grade differentials.
- **Tertiary (`#F59E0B`):** Warm amber/gold powers streak counters, rare achievement tiers, league leaderboards, and urgency prompts.
- **Quaternary Accent (`#0EA5E9`):** Soft sky/cyan indicates AI tutor presence, real-time contextual hints, and collaborative peer telemetry.
- **Neutral Canvas (`#F8FAFC`, `#FFFFFF`):** Base canvas and layered panel whites, framed by neutral slate dividers (`#E2E8F0`) and muted text tones (`#64748B`, `#0F172A`).

Color assignments must adhere to semantic intent. Amber is reserved for prestige, streaks, and badges; emerald is strictly reserved for success and verified learning outcomes.

## Typography

The typographic hierarchy uses **Plus Jakarta Sans** for headlines and standard body copy. It brings an open, modern geometry with friendly terminals that preserve readability across Latin and Cyrillic scripts (vital for Kazakh and Russian localization). 

**JetBrains Mono** is utilized selectively for telemetry: XP gains, streak numbers, analytical percentages, code modules, and technical gamification tiers. This technical monospace counterpoint gives the LMS an analytical, productivity-first feel.

Editorial guidelines:
- Maintain consistent optical weight when toggling between English, Russian, and Kazakh copy.
- Metric values and counter labels must always use tabular figures through the monospaced label stack to prevent layout jitter during live score/XP increments.

## Layout & Spacing

The layout is built upon a 12-column responsive fluid grid with strict max-width constraints on learning modules to ensure optimal reading line lengths (60–75 characters per line).

- **Desktop (1280px+):** 12-column grid, `2rem` outer margin, `1.5rem` gutters. Permanent or collapsible vertical command navigation (Linear-style sidebar) paired with a contextual workspace.
- **Tablet (768px - 1279px):** 8-column grid, `1.5rem` outer margins, `1rem` gutters. Sidebars fold into an off-canvas drawer or top toolbar.
- **Mobile (<768px):** 4-column grid, `1rem` margin, `0.75rem` gutters. Critical gamification stats (streaks, daily goal circles) dock to a streamlined top header, while navigation shifts to an anchored bottom tab bar.

Spatial relationships follow an 8pt base grid for structural containers and a 4pt grid for micro-elements (chips, badges, icon pairings).

## Elevation & Depth

This design system avoids heavy drop shadows, instead achieving hierarchy through tinted tonal surfaces, subtle outlines, and ambient color glows:

- **Surface Layering:** The primary background (`#F8FAFC`) hosts elevated white panels (`#FFFFFF`). Higher-tier overlays (floating AI tutor flyouts, mastery tooltips) use pure white surfaces bordered by `#E2E8F0`.
- **Low-Contrast Micro-Borders:** All cards, modules, and interactive controls leverage a `1px` solid border (`#E2E8F0` or `#CBD5E1`), giving components the tactile, deliberate structure of professional developer tools.
- **Gamified Luminescence:** High-impact milestones and streak nodes employ a subtle, diffused radial blur tinted with the accent's base hue:
  - Gold Streak Halo: `0 8px 24px -4px rgba(245, 158, 11, 0.20)`
  - Mastery Green Glow: `0 8px 24px -4px rgba(16, 185, 129, 0.20)`
  - Indigo Focus Depth: `0 4px 14px -2px rgba(79, 70, 229, 0.25)`

## Shapes

The design adopts a balanced **Level 2 (Rounded)** shape vocabulary to harmonize software efficiency with ergonomic learning touchpoints:

- Base cards, form fields, and modal containers use `0.5rem` (8px).
- Larger hero cards, lesson path containers, and interactive test boards use `1rem` (16px).
- Outer dashboard frames and modal viewports scale to `1.5rem` (24px).
- Streak chips, XP pills, avatar badges, and quick-filter switches use a continuous full-pill geometry (`9999px`) to maintain instant recognition as gamified interactive nodes.

## Components

### Buttons
- **Primary:** `#4F46E5` fill, `#FFFFFF` typography, subtle inset top border (`rgba(255,255,255,0.2)`), hover transition to `#4338CA`.
- **Gamified Action (Claim/Complete):** `#10B981` background with elevated bottom edge accentuation (`border-b-2 border-emerald-600`), providing tactile feedback without cartoonish extrusion.
- **Ghost/Tertiary:** Transparent fill, `1px` border in `#E2E8F0`, slate typography; hover produces `#F1F5F9` background.

### Badges & Streak Indicators
- Built with a full pill radius (`9999px`), displaying numbers in `JetBrains Mono`.
- **Streak Node:** `#FEF3C7` background, `#D97706` text, paired with an amber ember flame icon.
- **Language Switcher (KZ/RU/EN):** Minimal segmented control; selected language features a pure `#FFFFFF` chip with subtle drop shadow inside a `#F1F5F9` well.

### Progress Bars & Mastery Trackers
- Track: `#E2E8F0`, height of `6px` or `8px`, rounded to `9999px`.
- Fill: Smooth gradient transitioning from primary `#4F46E5` to accent `#10B981` upon milestone completion.
- Milestone ticks mark critical chapter increments along the track with small `4px` circular anchors.

### Cards
- Pure `#FFFFFF` surface with `1px solid #E2E8F0`. Padding defaults to `space-lg`.
- Hover state on lesson modules creates an active indigo border (`#4F46E5`) with a subtle `2px` upward translation.

### Form Inputs & Checkboxes
- **Input Fields:** Crisp `#FFFFFF` surface with `#CBD5E1` border, transitioning to a `2px` focus ring in `rgba(79, 70, 229, 0.2)` with a `#4F46E5` border.
- **Checkboxes & Radios:** `0.25rem` or rounded-full edges, activating with an emerald check (`#10B981`) upon correct quiz responses, or deep indigo (`#4F46E5`) for standard settings.

### AI Tutor Panel (Lumina Assistant)
- Floats along the lower-right workspace. Rimmed with a soft cyan edge (`#0EA5E9`), featuring a clean markdown output viewer and quick prompt chips for syntax help, translation, and concept summarization.