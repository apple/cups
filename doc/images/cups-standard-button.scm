;  CUPS Standard Button
;  Create a flat rounded button 

(define (script-fu-cups-standard-button text text-color button-height button-color bg-color)
  (let* (
  	(img (car (gimp-image-new 256 256 RGB)))
	(old-fg (car (gimp-palette-get-foreground)))
	(old-bg (car (gimp-palette-get-background)))
	(font-size (+ (/ (* 3 button-height) 5) 1))
	(dummy (gimp-palette-set-foreground text-color))
	(text-layer (car (gimp-text-fontname img -1 0 0 text 0
	                            TRUE font-size PIXELS
	                            "Sans L,")))
	(text-width (car (gimp-drawable-width text-layer)))
	(text-height (car (gimp-drawable-height text-layer)))
	(button-width (+ text-width button-height))
	(bg-layer (car (gimp-layer-new img button-width button-height
	                               RGBA-IMAGE "Background" 100
				       NORMAL-MODE)))
        )

    ; Disable undo while we do our work...
    (gimp-image-undo-disable img)

    ; Resize the image as needed...
    (gimp-image-resize img button-width button-height 0 0)
    (gimp-image-add-layer img bg-layer 1)
    (gimp-layer-set-preserve-trans text-layer TRUE)

    ; Clear the background...
    (gimp-selection-clear img)
    (gimp-palette-set-background bg-color)
    (gimp-edit-fill bg-layer 1)

    ; Make selections as needed for a rounded box.
    (gimp-rect-select img (* 0.5 button-height) 0
                      (- button-width button-height) button-height
		      REPLACE 0 0)
    (gimp-ellipse-select img (- button-width button-height) 0
                         button-height button-height ADD 1 0 0)
    (gimp-ellipse-select img 0 0 button-height button-height ADD 1 0 0)

    ; Fill in the background...
    (gimp-palette-set-background button-color)
    (gimp-edit-fill bg-layer 1)

    ; Clear the border around the button image...
    (gimp-selection-invert img)
    (gimp-edit-clear bg-layer)
    (gimp-selection-clear img)

    ; Restore original colors...
    (gimp-palette-set-foreground old-fg)
    (gimp-palette-set-background old-bg)

    ; Translate the text later to center it...
    (gimp-layer-translate text-layer (* 0.5 button-height)
                          (- (* 0.5 (- button-height text-height)) 1))

    ; Then flatten the image...
    (gimp-image-merge-visible-layers img CLIP-TO-IMAGE)
    (gimp-convert-indexed img 0 0 16 0 1 "")
    (gimp-image-undo-enable img)
    (gimp-display-new img)
  )
)

(script-fu-register "script-fu-cups-standard-button"
		    "<Toolbox>/Btns/CUPS Standard Button"
		    "CUPS Standard Button"
		    "Michael Sweet <mike@easysw.com>"
		    "Michael Sweet <mike@easysw.com>"
		    "2000"
		    ""
		    SF-VALUE "Text String" "\"Button\""
		    SF-COLOR "Text Color" '(255 255 255)
		    SF-VALUE "Button Size (in pixels)" "20"
		    SF-COLOR "Button Color" '(102 102 51)
		    SF-COLOR "Background Color" '(212 212 164)
)
