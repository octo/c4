var c4 =
{
  instances: []
};

function graph_get_params (graph)
{
  var graph_selector = graph.graph_selector;
  var inst_selector = graph.instance_selector;
  var selector = {};

  if (graph_selector.host == inst_selector.host)
  {
    selector.host = graph_selector.host;
  }
  else
  {
    selector.graph_host = graph_selector.host;
    selector.inst_host = inst_selector.host;
  }

  if (graph_selector.plugin == inst_selector.plugin)
  {
    selector.plugin = graph_selector.plugin;
  }
  else
  {
    selector.graph_plugin = graph_selector.plugin;
    selector.inst_plugin = inst_selector.plugin;
  }

  if (graph_selector.plugin_instance == inst_selector.plugin_instance)
  {
    selector.plugin_instance = graph_selector.plugin_instance;
  }
  else
  {
    selector.graph_plugin_instance = graph_selector.plugin_instance;
    selector.inst_plugin_instance = inst_selector.plugin_instance;
  }

  if (graph_selector.type == inst_selector.type)
  {
    selector.type = graph_selector.type;
  }
  else
  {
    selector.graph_type = graph_selector.type;
    selector.inst_type = inst_selector.type;
  }

  if (graph_selector.type_instance == inst_selector.type_instance)
  {
    selector.type_instance = graph_selector.type_instance;
  }
  else
  {
    selector.graph_type_instance = graph_selector.type_instance;
    selector.inst_type_instance = inst_selector.type_instance;
  }

  return (selector);
} /* graph_get_params */

function ident_clone (ident)
{
  var ret = {};

  ret.host = ident.host;
  ret.plugin = ident.plugin;
  ret.plugin_instance = ident.plugin_instance;
  ret.type = ident.type;
  ret.type_instance = ident.type_instance;

  return (ret);
} /* ident_clone */

function json_graph_get_def (graph)
{
  if (!graph.def)
  {
    var params = ident_clone (graph.graph_selector);
    params.action = "graph_def_json";

    $.ajax({
      url: "collection.fcgi",
      async: false,
      dataType: 'json',
      data: params,
      success: function (data)
      {
        if (!data)
          return;

        graph.def = data;
      }});
  }

  if (graph.def)
    return (graph.def);
  return;
} /* json_graph_get_def */

function json_graph_update (index)
{
  var graph;
  var def;
  var params;

  graph = c4.instances[index];
  if (!graph)
    return;

  def = json_graph_get_def (graph);
  if (!def)
    return;

  if (!graph.raphael)
  {
    graph.raphael = Raphael ("c4-graph" + index);
  }

  params = graph_get_params (graph);
  params.action = "graph_data_json";
  params.begin = graph.begin;
  params.end = graph.end;

  $.getJSON ("collection.fcgi", params,
      function (data)
      {
        var x_data = [];
        var y_data = [];
        var i;

        if (!data)
          return;

        for (i = 0; i < data.length; i++)
        {
          var ds = data[i];

          var j;
          var x = [];
          var y = [];

          for (j = 0; j < ds.data.length; j++)
          {
            var dp = ds.data[j];
            var t = dp[0];
            var v = dp[1];

            if (v == null)
              continue;

            x.push (t);
            y.push (v);
          }

          x_data.push (x);
          y_data.push (y);
        }

        graph.raphael.clear ();
        if (def.title)
          graph.raphael.g.text (250, 15, def.title);
        if (def.vertical_label)
          graph.raphael.g.text (5, 100, def.vertical_label).rotate (270);
        graph.raphael.g.linechart(50, 25, 500, 150, x_data, y_data, {axis: "0 0 1 1"});
      }); /* getJSON */
} /* json_graph_update */

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

function zoom_redraw (jq_obj) /* {{{ */
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
} /* }}} function zoom_redraw */

function zoom_reset (graph_id, diff) /* {{{ */
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
} /* }}} function zoom_reset */

function zoom_hour (graph_id) /* {{{ */
{
  zoom_reset (graph_id, 3600);
} /* }}} function zoom_hour */

function zoom_day (graph_id) /* {{{ */
{
  zoom_reset (graph_id, 86400);
} /* }}} function zoom_day */

function zoom_week (graph_id) /* {{{ */
{
  zoom_reset (graph_id, 7 * 86400);
} /* }}} function zoom_week */

function zoom_month (graph_id) /* {{{ */
{
  zoom_reset (graph_id, 31 * 86400);
} /* }}} function zoom_month */

function zoom_year (graph_id) /* {{{ */
{
  zoom_reset (graph_id, 366 * 86400);
} /* }}} function zoom_year */

function zoom_relative (graph_id, factor_begin, factor_end) /* {{{ */
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
} /* }}} function zoom_relative */

function zoom_reference (graph_id) /* {{{ */
{
  var jq_obj;
  var end;
  var begin;

  jq_obj = $("#" + graph_id);
  if (jq_obj == null)
    return (false);

  begin = jq_obj.data ('begin');
  end = jq_obj.data ('end');
  if ((begin == null) || (end == null))
    return (false);

  $(".graph-img img").each (function ()
  {
    $(this).data ('begin', begin);
    $(this).data ('end', end);
    zoom_redraw ($(this));
  });
} /* }}} function zoom_reference */

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
        + "<div class=\"graph-button\" onClick=\"zoom_reference ('"+id+"');\">!</div>"
        + "</div>"
        + "<div class=\"graph-buttons navigation\">"
        + "<div class=\"graph-button\" onClick=\"zoom_earlier ('"+id+"');\">←</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_out     ('"+id+"');\">−</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_in      ('"+id+"');\">+</div>"
        + "<div class=\"graph-button\" onClick=\"zoom_later   ('"+id+"');\">→</div>"
        + "</div>"
        );
    });

    var i;
    for (i = 0; i < c4.instances.length; i++)
    {
      json_graph_update (i);
    }
});

/* vim: set sw=2 sts=2 et fdm=marker : */
