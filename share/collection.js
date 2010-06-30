function format_instance(inst)
{
  return ("<li class=\"instance\"><a href=\"" + location.pathname
      + "?action=show_instance;" + inst.params + "\">" + inst.description
      + "</a></li>");
}

function format_instance_list(instances)
{
  var ret = "<ul class=\"instance_list\">";
  var i;

  if (instances.length == 0)
    return ("");

  for (i = 0; i < instances.length; i++)
    ret += format_instance (instances[i]);
  
  ret += "</ul>";

  return (ret);
}

function format_graph(graph)
{
  return ("<li class=\"graph\">" + graph.title + format_instance_list (graph.instances) + "</li>");
}

function update_search_suggestions ()
{
  var term = $("#search-input").val ();
  if (term.length < 2)
  {
    $("#search-suggest").hide ();
    return (true);
  }

  $("#search-suggest").show ();
  $.getJSON ("collection.fcgi",
    { "action": "search_json", "q": term},
    function(data)
    {
      var i;
      $("#search-suggest").html ("");
      for (i = 0; i < data.length; i++)
      {
        var graph = data[i];
        $("#search-suggest").append (format_graph (graph));
      }
    }
  );
} /* update_search_suggestions */

function zoom_redraw (jq_obj)
{
  var url = jq_obj.data ("base_url");

  if ((jq_obj == null) || (url == null))
    return (false);

  if (jq_obj.data ('begin') != null)
    url += ";begin=" + jq_obj.data ('begin');
  if (jq_obj.data ('end') != null)
    url += ";end=" + jq_obj.data ('end');

  jq_obj.attr ("src", url);
  return (true);
}

function zoom_reset (graph_id, diff)
{
  var jq_obj;
  var end;
  var begin;

  jq_obj = $("#" + graph_id);
  if (jq_obj == null)
    return (false);

  end = new Number ((new Date ()).getTime () / 1000);
  begin = new Number (end - diff);

  jq_obj.data ('begin', begin.toFixed (0));
  jq_obj.data ('end', end.toFixed (0));

  return (zoom_redraw (jq_obj));
}

function zoom_hour (graph_id)
{
  zoom_reset (graph_id, 3600);
}

function zoom_day (graph_id)
{
  zoom_reset (graph_id, 86400);
}

function zoom_week (graph_id)
{
  zoom_reset (graph_id, 7 * 86400);
}

function zoom_month (graph_id)
{
  zoom_reset (graph_id, 31 * 86400);
}

function zoom_year (graph_id)
{
  zoom_reset (graph_id, 366 * 86400);
}

function zoom_relative (graph_id, factor_begin, factor_end)
{
  var jq_obj;
  var end;
  var begin;
  var diff;

  jq_obj = $("#" + graph_id);
  if (jq_obj == null)
    return (false);

  begin = jq_obj.data ('begin');
  end = jq_obj.data ('end');
  if ((begin == null) || (end == null))
    return (zoom_day (graph_id));

  begin = new Number (begin);
  end = new Number (end);

  diff = end - begin;
  if ((diff <= 300) && (factor_begin > 0.0) && (factor_end < 0.0))
    return (true);

  jq_obj.data ('begin', begin + (diff * factor_begin));
  jq_obj.data ('end', end + (diff * factor_end));

  return (zoom_redraw (jq_obj));
}

function zoom_earlier (graph_id) /* {{{ */
{
  return (zoom_relative (graph_id, -0.2, -0.2));
} /* }}} function zoom_earlier */

function zoom_later (graph_id) /* {{{ */
{
  return (zoom_relative (graph_id, +0.2, +0.2));
} /* }}} function zoom_later */

function zoom_in (graph_id) /* {{{ */
{
  return (zoom_relative (graph_id, +0.2, -0.2));
} /* }}} function zoom_earlier */

function zoom_out (graph_id) /* {{{ */
{
  return (zoom_relative (graph_id, (-1.0 / 3.0), (1.0 / 3.0)));
} /* }}} function zoom_earlier */

$(document).ready(function() {
    /* $("#layout-middle-right").html ("<ul id=\"search-suggest\" class=\"graph_list\"></ul>"); */
    $("#search-form").append ("<ul id=\"search-suggest\" class=\"graph_list\"></ul>");
    $("#search-suggest").hide ();

    $("#search-input").blur (function()
    {
      window.setTimeout (function ()
      {
        $("#search-suggest").hide ();
      }, 500);
    });

    $("#search-input").focus (function()
    {
      var term = $("#search-input").val ();
      if (term.length < 2)
      {
        $("#search-suggest").hide ();
      }
      else
      {
        $("#search-suggest").show ();
      }
    });

    $("#search-input").keyup (function()
    {
      update_search_suggestions ();
    });

    var graph_count = 0;
    $(".graph-img").each (function (index, elem)
    {
      var id = "graph" + graph_count;
      graph_count++;

      $(this).find ("img").each (function (img_index, img_elem)
      {
        var base_url;

        $(this).attr ("id", id);

        base_url = $(this).attr ("src").replace (/;(begin|end)=[^;]*/g, '');
        $(this).data ("base_url", base_url);
      });

      $(this).append ("<div class=\"graph-buttons presets\">"
        + "<div class=\"graph-button\" onClick=\"zoom_hour  ('"+id+"');\">H</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_day   ('"+id+"');\">D</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_week  ('"+id+"');\">W</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_month ('"+id+"');\">M</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_year  ('"+id+"');\">Y</div>"
        + "<div class=\"graph-button\" >!</div>"
        + "</div>"
        + "<div class=\"graph-buttons navigation\">"
        + "<div class=\"graph-button\" onClick=\"zoom_earlier ('"+id+"');\">←</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_out     ('"+id+"');\">−</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_in      ('"+id+"');\">+</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_later   ('"+id+"');\">→</div>"
        + "</div>"
        );
    });
});

/* vim: set sw=2 sts=2 et fdm=marker : */
