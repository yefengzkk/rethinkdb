
render_body = ->
    template = Handlebars.compile $('#body-structure-template').html()
    $('body').html(template())
    # Set up common DOM behavior
    $('.modal').modal
        backdrop: true
        keyboard: true


class @IsDisconnected extends Backbone.View
        el: 'body'
        className: 'is_disconnected_view'
        template: Handlebars.compile $('#is_disconnected-template').html()
        message: Handlebars.compile $('#is_disconnected_message-template').html()
        initialize: =>
            log_initial '(initializing) sidebar view:'
            @render()

        render: =>
            @.$el.append @template
            @.$('.is_disconnected').modal(
                'show': true
                'backdrop': 'static'
            )
            @animate_loading()
            
        animate_loading: =>
            if @.$('.three_dots_connecting')
                if @.$('.three_dots_connecting').html() is '...'
                    @.$('.three_dots_connecting').html ''
                else
                    @.$('.three_dots_connecting').append '.'
                setTimeout(@animate_loading, 300)

        display_fail: =>
            @.$('.animation_state').fadeOut('slow', => 
                $('.reconnecting_state').html(@message)
                $('.animation_state').fadeIn('slow')
            )


