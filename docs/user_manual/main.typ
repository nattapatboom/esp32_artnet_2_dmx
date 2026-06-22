#set page(
  paper: "a4",
  margin: (x: 2.5cm, y: 2.5cm),
  header: align(right)[
    #text(8pt, fill: gray)[WT32-ETH01 DMX Lighting Node User Manual]
  ],
  footer: context {
    let page_number = counter(page).get().first()
    let total_pages = counter(page).final().first()
    align(center)[
      #text(9pt, fill: gray)[Page #page_number of #total_pages]
    ]
  }
)

#set text(
  font: ("Liberation Sans", "Arial", "Helvetica"),
  size: 11pt,
  lang: "en",
)

#set par(
  justify: true,
  leading: 0.65em,
)

#align(center)[
  #v(3cm)
  #text(size: 26pt, weight: "bold", fill: rgb("#1a5fb4"))[WT32-ETH01 DMX Lighting Node] \
  #v(0.5cm)
  #text(size: 18pt, weight: "medium", fill: rgb("#555555"))[User Manual \& Integration Guide] \
  #v(1.5cm)
  #text(size: 11pt, fill: gray)[Firmware Version 1.30.00] \
  #v(1cm)
  #text(size: 12pt)[Project Documentation] \
  #v(0.2cm)
  #text(size: 11pt, fill: gray)[#datetime.today().display("[day] [month repr:long] [year]")]
]

#v(4cm)
#line(length: 100%, stroke: 0.5pt + rgb("#e0e0e0"))
#v(1cm)

#outline(depth: 3, indent: 1.5em)

#pagebreak()

#set page(
  background: place(
    center + horizon,
    {
      let img = image("chal_logo.png", width: 10cm)
      context {
        let m = measure(img)
        stack(
          img,
          move(
            dy: -m.height,
            box(
              width: m.width,
              height: m.height,
              fill: white.transparentize(4%)
            )
          )
        )
      }
    }
  )
)

#include "chapters/quickstart.typ"
#include "chapters/network.typ"
#include "chapters/outputs.typ"
#include "chapters/espnow.typ"
#include "chapters/hardware.typ"
#include "chapters/troubleshooting.typ"
