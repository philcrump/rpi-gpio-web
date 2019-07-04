"use strict";

var hpa_power_state = false;

$(function() {

  $("#hpa-power-toggle").change(function()
  {
    if($(this).prop('checked'))
    {
      $.post( "/hpa_power_set", { state: "on" }, function( data ) {
        hpa_power_update(data.state);
      }, "json");
    }
    else
    {
      $.post( "/hpa_power_set", { state: "off" }, function( data ) {
        hpa_power_update(data.state);
      }, "json");
    }
  });

  $.get( "/hpa_power_set", function( data ) {
    hpa_power_update(data.state);
    if(data.state)
    {
      $("#hpa-power-toggle").bootstrapToggle('on');
    }
    else
    {
      $("#hpa-power-toggle").bootstrapToggle('off');
    }
  }, "json");

});


var hpa_power_badge;
function hpa_power_update(value)
{
  if(hpa_power_badge == undefined)
  {
    hpa_power_badge = $('#hpa-power-badge');
    hpa_power_badge.removeClass('badge-light');
  }

  if(value)
  {
    hpa_power_badge.removeClass('badge-secondary').addClass('badge-danger');
    hpa_power_badge.text("MAINS ON");
  }
  else
  {
    hpa_power_badge.removeClass('badge-danger').addClass('badge-secondary');
    hpa_power_badge.text("OFF");
  }
}